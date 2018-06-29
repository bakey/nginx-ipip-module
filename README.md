# nginx-ipip-module for Nginx

## Introduction

This module serves as a ip detect module

## Installation

   1. Configure Nginx adding this module with:
          
          ./configure --with-compat --add-dynamic-module=/path/to/nginx-ipip-module
       
   2. Build with `make modules`.
   
   3. Put the objs/ngx_http_ipip_module.so to the modules directory of Nginx
      
      Configure the module in the http conf.
      Example:

          load_module modules/ngx_http_ipip_module.so;
          http {
              ipip_db  ipip_db.datx;
          }

      Now doing something like:
          
          curl -i http://example.com/test
## code
   $ipip_country_name  -- Chinese name of country

   $ipip_region_name   -- Chinese province or region 

   $ipip_city_name     -- Chinese city

   $ipip_owner_domain  -- 

   $ipip_network_domain -- Chinese network domain name

   $ipip_latitude       -- latitude

   $ipip_longitude      -- longitude

   $ipip_timezone_city  -- English timezone city name

   $ipip_timezone       -- Timezone

   $ipip_china_admin_code -- China admin code

   $ipip_telecode         -- China tele code

   $ipip_country_code     -- Englisth country code

   $ipip_continent_code   -- Englisth continenet code

   $ipip_idc              -- IDC
   
   $ipip_basestation      -- Basestation of IP
   $ipip_anycast          -- Indicate a anycast IP
