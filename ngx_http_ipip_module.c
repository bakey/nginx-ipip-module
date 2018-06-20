/**
 * @brief  IpIp module for Nginx.
 *
 * @section LICENSE
 *
 * Copyright (C) 2011 by ipip.net
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
  */
#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

typedef unsigned char byte;
typedef unsigned int uint;
#define B2IL(b) (((b)[0] & 0xFF) | (((b)[1] << 8) & 0xFF00) | (((b)[2] << 16) & 0xFF0000) | (((b)[3] << 24) & 0xFF000000))
#define B2IU(b) (((b)[3] & 0xFF) | (((b)[2] << 8) & 0xFF00) | (((b)[1] << 16) & 0xFF0000) | (((b)[0] << 24) & 0xFF000000))

struct DBContext {
    byte *data;
    byte *index;
    uint *flag;
    uint offset;
} ;
char *strtok_r_2(char *str, char const *delims, char **context) {
    char *p = NULL, *ret = NULL;

    if (str != NULL) {
        *context = str;
    }

    if (*context == NULL) {
        return NULL;
    }

    if ((p = strpbrk(*context, delims)) != NULL) {
        *p = 0;
        ret = *context;
        *context = ++p;
    }
    else if (**context) {
        ret = *context;
        *context = NULL;
    }
    return ret;
}

static struct DBContext* init_db(const char* ipdb, int* error_code, ngx_conf_t *cf);
static int destroy(struct DBContext* ctx);
static int find_result_by_ip(const struct DBContext* ctx,const char *ip, char *result);
static ngx_int_t ngx_http_ipip_addr_str(ngx_http_request_t *r, char* ipstr);

typedef struct {
    struct DBContext   *db_ctx;
} ngx_http_ipip_conf_t;

int destroy(struct DBContext* ctx) {
    if (ctx->flag != NULL) {
        free(ctx->flag);
    }
    if (ctx->index != NULL) {
        free(ctx->index);
    }
    if (ctx->data != NULL) {
        free(ctx->data);
    }
    ctx->offset = 0;
    free(ctx);
    return 0;
}

struct DBContext* init_db(const char* ipdb, int* error_code, ngx_conf_t *cf) {
    int read_count = 0;
    int copy_bytes = 0;
    struct DBContext* ctx = (struct DBContext *)malloc(sizeof(struct DBContext));
    if (ctx == NULL) {
        (*error_code) = 1;
        return NULL;
    }
    ctx->data = NULL;
    ctx->index = NULL;
    ctx->flag = NULL;
    ctx->offset = 0;

    FILE *file = fopen(ipdb, "rb");
    if (file == NULL) {
        (*error_code) = 2;
        free(ctx);
        return NULL;
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    ctx->data = (byte *) malloc(size * sizeof(byte));
    read_count = fread(ctx->data, sizeof(byte), (size_t) size, file);
    if (read_count <= 0) {
        (*error_code) = 3;
        free(ctx->data);
        free(ctx);
        return NULL;
    }
    ngx_conf_log_error(NGX_LOG_NOTICE, cf, 0, "total read %d bytes data", read_count);
    
    fclose(file);
    
    uint indexLength = B2IU(ctx->data);

    ngx_conf_log_error(NGX_LOG_NOTICE, cf, 0, "index len = %d", indexLength);
    
    ctx->index = (byte *) malloc(indexLength * sizeof(byte));
    if (ctx->index == NULL) {
        (*error_code) = 4;
        free(ctx->data);
        free(ctx);
        return NULL;
    }
    if (indexLength > size - 4) {
        copy_bytes = size - 4;
    } else {
        copy_bytes = indexLength;
    }

    ngx_memcpy(ctx->index, ctx->data + 4, copy_bytes);
    
    ctx->offset = indexLength;
    
    int flag_bytes = 65536 * sizeof(uint);
    ctx->flag = (uint *) malloc(flag_bytes);
    if (copy_bytes > flag_bytes) {
        copy_bytes = flag_bytes;
    }
    ngx_conf_log_error(NGX_LOG_NOTICE, cf, 0, "copy %d bytes from index to flag", copy_bytes);
    ngx_memcpy(ctx->flag, ctx->index, copy_bytes);
    
    return ctx;
}

static int find_result_by_ip(const struct DBContext* ctx, const char *ip, char *result) {
    uint ips[4];
    int num = sscanf(ip, "%d.%d.%d.%d", &ips[0], &ips[1], &ips[2], &ips[3]);
    if (num == 4) {
        uint ip_prefix_value = ips[0] * 256 + ips[1];
        uint ip2long_value = B2IU(ips);
        uint start = ctx->flag[ip_prefix_value];
        uint max_comp_len = ctx->offset - 262144 - 4;
        uint index_offset = 0;
        uint index_length = 0;
        for (start = start * 9 + 262144; start < max_comp_len; start += 9) {
            if (B2IU(ctx->index + start) >= ip2long_value) {
                index_offset = B2IL(ctx->index + start + 4) & 0x00FFFFFF;
                index_length = (ctx->index[start+7] << 8) + ctx->index[start+8];
                break;
            }
        }
        memcpy(result, ctx->data + ctx->offset + index_offset - 262144, index_length);
        result[index_length] = '\0';
    }
    return 0;
}

/*char *ngx_ip_result_desc[] = {
    "Country Name",
    "Region Name",
    "City Name"
};*/

ngx_module_t ngx_http_ipip_module;
static ngx_int_t get_element(ngx_http_request_t *r, char* result, 
    struct DBContext *db_ctx, int index) {
    char ipstr[32];
    ngx_http_ipip_addr_str(r, ipstr);
    find_result_by_ip(db_ctx, ipstr, result);

    char *rst = NULL;
    char *lasts = NULL;
    rst = strtok_r_2(result, "\t", &lasts);
    int cnt = 0;
    while (rst) {
        if (index == cnt) {
            ngx_memcpy(result, rst, ngx_strlen(rst));
            break;
        }
        rst = strtok_r_2(NULL, "\t", &lasts);
        ++ cnt;
    }

    return NGX_OK;
}

static ngx_int_t ngx_ipip_set_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, int index) {
    ngx_http_ipip_conf_t  *icf = ngx_http_get_module_main_conf(r, ngx_http_ipip_module);

    char result[1024];
    size_t val_len;

    int ret = get_element(r, result, icf->db_ctx, index);
    if (ret != 0) {
        return NGX_ERROR;
    }

    val_len = ngx_strlen(result);
    v->data = ngx_pnalloc(r->pool, val_len + 1);
    if (v->data == NULL) {
        return NGX_ERROR;
    }

    ngx_memcpy(v->data, result, val_len);
    v->data[val_len] = '\0';

    v->len = val_len;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;

    return NGX_OK;
}

#define NGX_IPIP_COUNTRY_NAME_CODE     0
#define NGX_IPIP_REGION_NAME_COEE      1
#define NGX_IPIP_CITY_NAME_CODE        2
#define NGX_IPIP_OWNER_DOMAIN_CODE     3
#define NGX_IPIP_NETWORK_DOMAIN_CODE   4
#define NGX_IPIP_LATITUDE_CODE         5

#define NGX_IPIP_LONGITUDE_CODE        6
#define NGX_IPIP_TIMEZONE_CITY_CODE    7
#define NGX_IPIP_TIMEZONE_CODE         8

#define NGX_IPIP_CHINA_ADMIN_CODE      9
#define NGX_IPIP_TELECODE_CODE         10
#define NGX_IPIP_COUNTRYCODE_CODE      11
#define NGX_IPIP_CONTINENT_CODE        12

#define NGX_IPIP_IDC_VPN_CODE          13
#define NGX_IPIP_BASESTATION_CODE      14

static char *ngx_http_ipip_db(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

static ngx_int_t ngx_http_ipip_add_variables(ngx_conf_t *cf);

static ngx_int_t ngx_http_ipip_country_variable(ngx_http_request_t *r, 
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_ipip_region_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_ipip_city_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_ipip_owner_domain_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_ipip_network_domain_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_ipip_latitude_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_ipip_longitude_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_ipip_timezone_city_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);

static ngx_int_t ngx_http_ipip_timezone_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_ipip_china_admin_code_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_ipip_telecode_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);

static ngx_int_t ngx_http_ipip_country_code_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_ipip_continent_code_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_ipip_idc_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_ipip_basestation_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);


static void *ngx_http_ipip_create_conf(ngx_conf_t *cf);
static void ngx_http_ipip_cleanup(void *data);
/**
 * This module provided directive: ipip.
 *
 */
static ngx_command_t ngx_http_ipip_commands[] = {

    { ngx_string("ipip_db"), /* directive */
      NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1, /* location context and takes
                                            no arguments*/
      ngx_http_ipip_db, /* configuration setup function */
      NGX_HTTP_MAIN_CONF_OFFSET, /* No offset. Only one context is supported. */
      0, /* No offset when storing the module configuration on struct. */
      NULL},

    ngx_null_command /* command termination */
};

/* The module context. */
static ngx_http_module_t ngx_http_ipip_module_ctx = {
    ngx_http_ipip_add_variables, /* preconfiguration */
    NULL, /* postconfiguration */

    ngx_http_ipip_create_conf, /* create main configuration */
    NULL, /* init main configuration */

    NULL, /* create server configuration */
    NULL, /* merge server configuration */

    NULL, /* create location configuration */
    NULL /* merge location configuration */
};

/* Module definition. */
ngx_module_t ngx_http_ipip_module = {
    NGX_MODULE_V1,
    &ngx_http_ipip_module_ctx, /* module context */
    ngx_http_ipip_commands, /* module directives */
    NGX_HTTP_MODULE, /* module type */
    NULL, /* init master */
    NULL, /* init module */
    NULL, /* init process */
    NULL, /* init thread */
    NULL, /* exit thread */
    NULL, /* exit process */
    NULL, /* exit master */
    NGX_MODULE_V1_PADDING
};

static ngx_http_variable_t  ngx_http_ipip_vars[] = {

    { ngx_string("ipip_country_name"), NULL,
      ngx_http_ipip_country_variable,
      0, 0, 0 },

    { ngx_string("ipip_region_name"), NULL,
      ngx_http_ipip_region_variable,
      0, 0, 0 },

    { ngx_string("ipip_city_name"), NULL,
      ngx_http_ipip_city_variable,
      0, 0, 0 },

    { ngx_string("ipip_owner_domain"), NULL,
      ngx_http_ipip_owner_domain_variable,
      0, 0, 0 },

    { ngx_string("ipip_network_domain"), NULL,
      ngx_http_ipip_network_domain_variable,
      0, 0, 0 },

      { ngx_string("ipip_latitude"), NULL,
      ngx_http_ipip_latitude_variable,
      0, 0, 0 },

      { ngx_string("ipip_longitude"), NULL,
      ngx_http_ipip_longitude_variable,
      0, 0, 0 },

      { ngx_string("ipip_timezone_city"), NULL,
      ngx_http_ipip_timezone_city_variable,
      0, 0, 0 },

      { ngx_string("ipip_timezone"), NULL,
      ngx_http_ipip_timezone_variable,
      0, 0, 0 },

      { ngx_string("ipip_china_admin_code"), NULL,
      ngx_http_ipip_china_admin_code_variable,
      0, 0, 0 },

      { ngx_string("ipip_telecode"), NULL,
      ngx_http_ipip_telecode_variable,
      0, 0, 0 },

      { ngx_string("ipip_country_code"), NULL,
      ngx_http_ipip_country_code_variable,
      0, 0, 0 },

      { ngx_string("ipip_continent_code"), NULL,
      ngx_http_ipip_continent_code_variable,
      0, 0, 0 },

      { ngx_string("ipip_idc"), NULL,
      ngx_http_ipip_idc_variable,
      0, 0, 0 },

      { ngx_string("ipip_basestation"), NULL,
      ngx_http_ipip_basestation_variable,
      0, 0, 0 },

    { ngx_null_string, NULL, NULL, 0, 0, 0 }
};

static ngx_int_t
ngx_http_ipip_addr_str(ngx_http_request_t *r, char* ipstr)
{
    ngx_addr_t           addr;
    struct sockaddr_in  *sin;

    addr.sockaddr = r->connection->sockaddr;
    addr.socklen = r->connection->socklen;

    if (addr.sockaddr->sa_family != AF_INET) {
        return INADDR_NONE;
    }

    sin = (struct sockaddr_in *) addr.sockaddr;
    inet_ntop(AF_INET, &(sin->sin_addr), ipstr, INET_ADDRSTRLEN);

    return NGX_OK;
}

static void
ngx_http_ipip_cleanup(void *data)
{
    ngx_http_ipip_conf_t  *icf = data;

    if (icf->db_ctx) {
        destroy(icf->db_ctx);
        icf->db_ctx = NULL;
    }
}

static void *
ngx_http_ipip_create_conf(ngx_conf_t *cf)
{
    ngx_pool_cleanup_t     *cln;
    ngx_http_ipip_conf_t  *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_ipip_conf_t));
    if (conf == NULL) {
        return NULL;
    }

    cln = ngx_pool_cleanup_add(cf->pool, 0);
    if (cln == NULL) {
        return NULL;
    }

    cln->handler = ngx_http_ipip_cleanup;
    cln->data = conf;

    return conf;
}

static ngx_int_t
ngx_http_ipip_region_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data) {

    return ngx_ipip_set_variable(r, v, NGX_IPIP_REGION_NAME_COEE);
}
static ngx_int_t 
ngx_http_ipip_city_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data) {

    return ngx_ipip_set_variable(r, v, NGX_IPIP_CITY_NAME_CODE);
}
static ngx_int_t 
ngx_http_ipip_owner_domain_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data) {
    return ngx_ipip_set_variable(r, v, NGX_IPIP_OWNER_DOMAIN_CODE);
}
static ngx_int_t ngx_http_ipip_network_domain_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data) {
    return ngx_ipip_set_variable(r, v, NGX_IPIP_NETWORK_DOMAIN_CODE);
}
static ngx_int_t ngx_http_ipip_latitude_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data) {
    return ngx_ipip_set_variable(r, v, NGX_IPIP_LATITUDE_CODE);
}
static ngx_int_t ngx_http_ipip_longitude_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data) {
    return ngx_ipip_set_variable(r, v, NGX_IPIP_LONGITUDE_CODE);
}
static ngx_int_t
ngx_http_ipip_country_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data) {
    return ngx_ipip_set_variable(r, v, NGX_IPIP_COUNTRY_NAME_CODE);
}
static ngx_int_t
ngx_http_ipip_timezone_city_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data) {
    return ngx_ipip_set_variable(r, v, NGX_IPIP_TIMEZONE_CITY_CODE);
}

static ngx_int_t ngx_http_ipip_timezone_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data) {
    return ngx_ipip_set_variable(r, v, NGX_IPIP_TIMEZONE_CODE);
}
static ngx_int_t ngx_http_ipip_china_admin_code_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data) {
    return ngx_ipip_set_variable(r, v, NGX_IPIP_CHINA_ADMIN_CODE);
}
static ngx_int_t ngx_http_ipip_telecode_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data) {
    return ngx_ipip_set_variable(r, v, NGX_IPIP_TELECODE_CODE);
}

static ngx_int_t ngx_http_ipip_country_code_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data) {
    return ngx_ipip_set_variable(r, v, NGX_IPIP_COUNTRYCODE_CODE);
}
static ngx_int_t ngx_http_ipip_continent_code_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data) {
    return ngx_ipip_set_variable(r, v, NGX_IPIP_CONTINENT_CODE);
}
static ngx_int_t ngx_http_ipip_idc_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data) {
    return ngx_ipip_set_variable(r, v, NGX_IPIP_IDC_VPN_CODE);
}
static ngx_int_t ngx_http_ipip_basestation_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data) {
    return ngx_ipip_set_variable(r, v, NGX_IPIP_BASESTATION_CODE);
}

static ngx_int_t
ngx_http_ipip_add_variables(ngx_conf_t *cf)
{
    ngx_http_variable_t  *var, *v;

    for (v = ngx_http_ipip_vars; v->name.len; v++) {
        var = ngx_http_add_variable(cf, &v->name, v->flags);
        if (var == NULL) {
            return NGX_ERROR;
        }

        var->get_handler = v->get_handler;
        var->data = v->data;
    }
    ngx_conf_log_error(NGX_LOG_NOTICE, cf, 0,
                               "enter ipip add variables");

    return NGX_OK;
}

/**
 * Configuration setup function that installs the content handler.
 *
 * @param cf
 *   Module configuration structure pointer.
 * @param cmd
 *   Module directives structure pointer.
 * @param conf
 *   Module configuration structure pointer.
 * @return string
 *   Status of the configuration setup.
 */
static char *ngx_http_ipip_db(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_str_t  *value;
    ngx_http_ipip_conf_t *icf = conf;

    value = cf->args->elts;
    int error_code = 0;
    icf->db_ctx = init_db((char *) value[1].data, &error_code, cf);

    if (icf->db_ctx != NULL) {
        ngx_conf_log_error(NGX_LOG_NOTICE, cf, 0,
                               "ipip open db = %s success", (char *) value[1].data);
    } else {
        ngx_conf_log_error(NGX_LOG_NOTICE, cf, 0,
                               "ipip open db = %s failed, error code = %d",
                               (char *) value[1].data, error_code);
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
} /* ngx_http_ipip_db */
