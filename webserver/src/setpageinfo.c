
#include <string.h>

#include "setpageinfo.h"

int setpageinfo(int status_code, struct pageinfo *pi)
{
        switch(status_code) {
        case 500:
                pi->filename = strdup("500_internal_server_error.html"); // ska kanske i textform
                pi->status = strdup("500 Internal Server Error");
                break;
        case 200:
                pi->status = strdup("200 OK");
                break;
        case 400:
                pi->filename = strdup("400_bad_request.html");
                pi->status = strdup("400 Bad Request");
                break;
        case 403:
                pi->filename = strdup("403_forbidden.html");
                pi->status = strdup("403 Forbidden");
                break;
        case 404:
                pi->filename = strdup("404_not_found.html");
                pi->status = strdup("404 Not Found");
                break;
        case 501:
                pi->filename = strdup("501_not_implemented.html");
                pi->status = strdup("501 Not Implemented");
                break;
        default:
                return 1;
        }

        return 0;
}
