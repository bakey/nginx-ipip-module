# nginx-ipip-module for Nginx

## Introduction

This module serves as a ip detect module

## Installation

   1. Configure Nginx adding this module with:
          
          ./configure --with-compat --add-module=/path/to/nginx-ipip-module
       
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
