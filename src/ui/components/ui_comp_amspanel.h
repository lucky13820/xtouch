#ifndef _UI_COMP_AMSPANEL_H
#define _UI_COMP_AMSPANEL_H

#include "../ui.h"

#ifdef __cplusplus
extern "C"
{
#endif

    lv_obj_t *ui_amsPanel_create(lv_obj_t *comp_parent);
    void ui_amsPanel_show();
    void ui_amsPanel_hide();

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
