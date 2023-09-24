#include "https_frontend.h"

https_frontend::~https_frontend() {
    if (this->ssl != nullptr)
        SSL_free(this->ssl);
}
bool https_frontend::on_open() {
   this->ssl = ssl::on_accepted(this->get_fd(), ssl::ctx);
   if (this->ssl == nullptr)
       return false;

   // add read ev
   return true;
}
