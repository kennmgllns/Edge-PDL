
#pragma once

#define K_WEB_SERVER_BASIC_AUTH

#ifdef K_WEB_SERVER_BASIC_AUTH
  #define K_WEB_SERVER_AUTH_USERNAME            "admin"
  #define K_WEB_SERVER_AUTH_PASSWORD            "edge1234"
  #define K_WEB_SERVER_AUTH_REALM               "login ka muna"
#endif // K_WEB_SERVER_BASIC_AUTH

#define K_WEB_SERVER_MAX_URI_HANDLERS           (20) //8 

