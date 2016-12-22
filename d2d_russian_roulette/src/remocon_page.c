#include "d2d_russian_roulette.h"

extern appdata_s *__g_ad;

extern void deinit_d2d();
extern void show_gameplay_page();
extern void rss_start_service_foreach_cb(conv_service_h service_h, void* user_data);

static Eina_Bool page_remocon_pop_callback(void *data, Elm_Object_Item *it)
{
	deinit_d2d();
	ui_app_exit();
	return EINA_FALSE;
}

void
remocon_list_selected_callback(void *data, Evas_Object *obj, void *event_info)
{
	elm_list_item_selected_set((Elm_Object_Item *)event_info, EINA_FALSE);

	__g_ad->selected_device_h = (conv_device_h)data;
	if (__g_ad->selected_device_h) {
		/* The user chose the device, so discovery is not needed any more */
		conv_discovery_stop(__g_ad->conv_h);

		/*
		 * Use remote device as a remote control.
		 * show_gameplay_page() is deffered untill the RSS is started.
		 */
		conv_device_foreach_service(__g_ad->selected_device_h, rss_start_service_foreach_cb, NULL);
	} else
		/* No remote control. Go directly to the game play page */
		show_gameplay_page();
}

void
show_remocon_page()
{
	Evas_Object *remocon_page = elm_grid_add(__g_ad->nf);
	evas_object_show(remocon_page);

	Evas_Object *remocon_list_frame = elm_bg_add(remocon_page);
	elm_grid_pack(remocon_page, remocon_list_frame, 0, 0, 100, 100);
	evas_object_show(remocon_list_frame);

	__g_ad->remocon_list = elm_list_add(remocon_list_frame);
	elm_object_content_set(remocon_list_frame, __g_ad->remocon_list);
	elm_list_mode_set(__g_ad->remocon_list, ELM_LIST_COMPRESS);

	/* This is a default value: no remote control */
	elm_list_item_append(__g_ad->remocon_list, "No remocon", NULL, NULL, remocon_list_selected_callback, NULL);

	elm_list_go(__g_ad->remocon_list);

	Elm_Object_Item *nf_it = elm_naviframe_item_push(__g_ad->nf, "Select Remote Control", NULL, NULL, remocon_page, NULL);
	elm_naviframe_item_pop_cb_set(nf_it, page_remocon_pop_callback, NULL);
}
