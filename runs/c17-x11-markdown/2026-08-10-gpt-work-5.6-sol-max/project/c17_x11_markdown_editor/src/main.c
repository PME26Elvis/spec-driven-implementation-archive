#include "mdedit/ui.h"

#include <stdio.h>

int main(int argc,char **argv) {
    MdApp *app=md_app_create();
    if (app==NULL) {
        fputs("mdeditor: cannot initialize application state\n",stderr);
        return 1;
    }
    int result=md_app_run(app,argc,argv);
    md_app_destroy(app);
    return result;
}
