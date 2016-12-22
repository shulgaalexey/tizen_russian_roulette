#include "d2d_russian_roulette.h"

extern appdata_s *__g_ad;

extern void deinit_d2d();
extern void toggle_accelerometer_sensor(bool start);

static Eina_Bool
__test_timer_cb(void *data)
{
	dlog_print(DLOG_INFO, LOG_TAG, "on timer: remaining %d",
			__g_ad->timer_ticks_remaining);

	__g_ad->timer_ticks_remaining--;

	if (__g_ad->timer_ticks_remaining < 0) {
		ecore_timer_del(__g_ad->timer);
		__g_ad->timer = NULL;
		__g_ad->main_button_lock = false;
		elm_object_text_set(__g_ad->main_button, "Pull the trigger!!!");
		return ECORE_CALLBACK_CANCEL;
	}

	/* Redrawing the bullit in the cylinder */
	Elm_Object_Item *item = elm_list_selected_item_get(__g_ad->revolver_list);
	if (item) {
		elm_list_item_selected_set(item, EINA_FALSE);
		item = elm_list_item_next(item);
		if (!item)
			item = elm_list_first_item_get(__g_ad->revolver_list);
		elm_list_item_selected_set(item, EINA_TRUE);
	} else {
		/*
		 * This is an error case, because it is supposed that selection
		 * is always exists.
		 */
		item = elm_list_first_item_get(__g_ad->revolver_list);
		elm_list_item_selected_set(item, EINA_TRUE);
	}

	return ECORE_CALLBACK_RENEW;
}

void
on_roll_cylinder()
{
	if (__g_ad->main_button_lock)
		return;

	__g_ad->main_button_lock = true;

	elm_object_text_set(__g_ad->main_button, "rolling...");

	/* Set accelerometer on pause */
	toggle_accelerometer_sensor(false);

	/* 5 complete loops for drama + some random value for positioning the bullit */
	__g_ad->timer_ticks_remaining = 30 + rand() % 6;

	__g_ad->timer = ecore_timer_add(0.2, __test_timer_cb, NULL);
}

void
on_pull_trigger()
{
	if (__g_ad->main_button_lock)
		return;

	if (elm_list_selected_item_get(__g_ad->revolver_list) ==
			elm_list_first_item_get(__g_ad->revolver_list)) {
		// GAME OVER
		dlog_print(DLOG_INFO, LOG_TAG, "***** GAME OVER *****");
		elm_object_text_set(__g_ad->main_button, "GAME OVER");

		if (__g_ad->selected_rac_service_h) {
			app_control_h app_control_h = NULL;
			app_control_create(&app_control_h);
			app_control_set_operation(app_control_h, APP_CONTROL_OPERATION_VIEW);
			app_control_set_uri(app_control_h, "file://home/owner/media/Sounds/cosmo.wav");

			conv_payload_h payload = NULL;
			conv_payload_create(&payload);
			conv_payload_set_app_control(payload, "app_control", app_control_h);

			conv_service_publish(__g_ad->selected_rac_service_h, NULL, payload);

			conv_payload_destroy(payload);
			app_control_destroy(app_control_h);
		}
	} else {
		elm_object_text_set(__g_ad->main_button, "Roll");

		/* Restart the accelerometer */
		toggle_accelerometer_sensor(true);
	}

	// TODO How to disable the button????
	//elm_object_item_disabled_set(__g_ad->main_button, EINA_FALSE);
	//elm_object_style_set(__g_ad->main_button, "disabled");
}

static void
main_button_click_event(void *data, Evas_Object *obj, void *event_info)
{
	const char *btn_text = elm_object_text_get(__g_ad->main_button);
	if (!strcmp(btn_text, "Roll"))
		on_roll_cylinder();
	else if (!strcmp(btn_text, "Pull the trigger!!!"))
		on_pull_trigger();
	else if (!strcmp(btn_text, "GAME OVER"))
		ui_app_exit();
}

static Eina_Bool page_gameplay_pop_callback(void *data, Elm_Object_Item *it)
{
	if (__g_ad->timer) {
		ecore_timer_del(__g_ad->timer);
		__g_ad->timer = NULL;
	}

	/*
	 * It is possible to switch to the Selecting RemoCon page,
	 * but for simplicity, the app is terminated.
	 */
	deinit_d2d();
	ui_app_exit();
	return EINA_FALSE;
}

static void
revolver_list_selected_callback(void *data, Evas_Object *obj, void *event_info)
{
	// Empty
}

void
show_gameplay_page()
{
	Evas_Object *gameplay_page = elm_grid_add(__g_ad->nf);
	evas_object_show(gameplay_page);

	Evas_Object *revolver_list_frame = elm_bg_add(gameplay_page);
	elm_grid_pack(gameplay_page, revolver_list_frame, 0, 0, 100, 100);
	evas_object_show(revolver_list_frame);

	__g_ad->revolver_list = elm_list_add(revolver_list_frame);
	elm_object_content_set(revolver_list_frame, __g_ad->revolver_list);
	elm_list_mode_set(__g_ad->revolver_list, ELM_LIST_COMPRESS);
	const int bullit_pos = rand() % 6;
	for (int i = 0; i < 6; i++) {
		Elm_Object_Item *item = elm_list_item_append(__g_ad->revolver_list,
				((i)
						? "[ -------- ]"				/* Empty barrel of cylinder */
						: "[ -------- ]     ======^"),	/* Muzzle-sight */
						NULL, NULL, revolver_list_selected_callback, (const int *)0);
		if (i == bullit_pos)
			elm_list_item_selected_set(item, EINA_TRUE);
	}
	elm_list_go(__g_ad->revolver_list);

	__g_ad->main_button = elm_button_add(gameplay_page);
	elm_grid_pack(gameplay_page, __g_ad->main_button, 0, 85, 100, 15);
	elm_object_text_set(__g_ad->main_button, "Roll");
	evas_object_smart_callback_add(__g_ad->main_button, "clicked", main_button_click_event, NULL);
	evas_object_show(__g_ad->main_button);

	Elm_Object_Item *nf_it = elm_naviframe_item_push(__g_ad->nf, "Charge the bullit", NULL, NULL, gameplay_page, NULL);
	elm_naviframe_item_pop_cb_set(nf_it, page_gameplay_pop_callback, NULL);
}
