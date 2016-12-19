#include "d2d_russian_roulette.h"
#include <glib.h>
#include <d2d_conv_manager.h>

static const int CONV_SERVICE_REMOTE_SENSOR = 3;

typedef struct appdata {
	Evas_Object *win;
	Evas_Object *conform;
	Evas_Object *nf;
	Evas_Object *remocon_list;
	Evas_Object *main_button;
	bool main_button_lock;
	conv_h conv_h;
	conv_device_h selected_device_h;
	conv_service_h selected_service_h;
} appdata_s;

appdata_s *__g_ad = NULL;

static void show_remocon_page();
static void show_gameplay_page();

static void
win_delete_request_cb(void *data, Evas_Object *obj, void *event_info)
{
	ui_app_exit();
}

static void
win_back_cb(void *data, Evas_Object *obj, void *event_info)
{
	appdata_s *ad = data;
	/* Let window go to hide state. */
	elm_win_lower(ad->win);
}

static void
create_base_gui(appdata_s *ad)
{
	/* Window */
	/* Create and initialize elm_win.
	   elm_win is mandatory to manipulate window. */
	ad->win = elm_win_util_standard_add(PACKAGE, PACKAGE);
	elm_win_autodel_set(ad->win, EINA_TRUE);

	if (elm_win_wm_rotation_supported_get(ad->win)) {
		int rots[4] = { 0, 90, 180, 270 };
		elm_win_wm_rotation_available_rotations_set(ad->win, (const int *)(&rots), 4);
	}

	evas_object_smart_callback_add(ad->win, "delete,request", win_delete_request_cb, NULL);
	eext_object_event_callback_add(ad->win, EEXT_CALLBACK_BACK, win_back_cb, ad);

	/* Conformant */
	/* Create and initialize elm_conformant.
	   elm_conformant is mandatory for base gui to have proper size
	   when indicator or virtual keypad is visible. */
	ad->conform = elm_conformant_add(ad->win);
	elm_win_indicator_mode_set(ad->win, ELM_WIN_INDICATOR_SHOW);
	elm_win_indicator_opacity_set(ad->win, ELM_WIN_INDICATOR_OPAQUE);
	evas_object_size_hint_weight_set(ad->conform, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	elm_win_resize_object_add(ad->win, ad->conform);
	evas_object_show(ad->conform);

	/* Naviframe */
	ad->nf = elm_naviframe_add(ad->conform);
	eext_object_event_callback_add(ad->nf, EEXT_CALLBACK_BACK, eext_naviframe_back_cb, ad);
	evas_object_size_hint_weight_set(ad->nf, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	elm_object_content_set(ad->conform, ad->nf);
	evas_object_show(ad->nf);

	/* Show window after base gui is set up */
	evas_object_show(ad->win);
}

static void
remote_sensor_listener_cb(conv_service_h service_h, conv_channel_h channel_h,
		conv_error_e error, conv_payload_h payload_h, void* user_data)
{
	//dlog_print(DLOG_INFO, LOG_TAG, "remote_sensor_listener_cb()");

	// That clonned handle is a handle we should use.
	// The handle, passed to the callback is temporary within the callback scope.
	conv_service_h service_clone_h = __g_ad->selected_service_h;

	if (!service_clone_h || !service_h || (error != CONV_ERROR_NONE) || !payload_h) {
		dlog_print(DLOG_ERROR, LOG_TAG, "Invalid arguments of listener callback:");
		dlog_print(DLOG_ERROR, LOG_TAG, "   cloned service handle: %p", service_clone_h);
		dlog_print(DLOG_ERROR, LOG_TAG, "   service handle: %p", service_h);
		dlog_print(DLOG_ERROR, LOG_TAG, "   Response error [%d]", error);
		dlog_print(DLOG_ERROR, LOG_TAG, "   payload_handle %p", payload_h);
		return; // Too bad, arguments are invalid
	}

	char *result_type = NULL;
	int ret = conv_payload_get_string(payload_h, "result_type", &result_type);
	if (ret != CONV_ERROR_NONE) {
		dlog_print(DLOG_ERROR, LOG_TAG, "conv_payload_get_string Error getting result_type from payload_handle [%d]", ret);
		return;
	}

	if (!(strcmp(result_type, "onStart"))) { // 1. Service started
		dlog_print(DLOG_INFO, LOG_TAG, "The Service started!");
		dlog_print(DLOG_INFO, LOG_TAG, "Reading the Accel and Proxi data...");
		conv_payload_h payload_h = NULL;

		// Preparing and sending read request
		conv_payload_create(&payload_h);
		conv_payload_set_string(payload_h, "periodic", "yes");
		conv_payload_set_string(payload_h, "interval", "1000");

		conv_channel_set_string(channel_h, "type", "2"); // Pure acceleration
		//ret = conv_channel_set_string(channel_h, "type", "0");
		ret = conv_service_read(service_clone_h, channel_h, payload_h);
		if (ret != CONV_ERROR_NONE)
			dlog_print(DLOG_ERROR, LOG_TAG, "conv_service_read error [%d]", ret);

		conv_channel_set_string(channel_h, "type", "8"); // Proximity
		ret = conv_service_read(service_clone_h, channel_h, payload_h);
		if (ret != CONV_ERROR_NONE)
			dlog_print(DLOG_ERROR, LOG_TAG, "conv_service_read error [%d]", ret);

		conv_payload_destroy(payload_h);
	} else if (!strcmp(result_type, "onSuccess")) { // 2. Read request processed sucessfully
		dlog_print(DLOG_INFO, LOG_TAG, "Read request successful : %s Result", result_type);

		char *channel_type = NULL;
		conv_channel_get_string(channel_h, "type", &channel_type);
		dlog_print(DLOG_INFO, LOG_TAG, "Channel read request processed successfully : [%s]", channel_type);

		static bool accelerometer_started = false;
		static bool proximity_started = false;

		if (!strcmp(channel_type, "2"))
			accelerometer_started = true;
		if (!strcmp(channel_type, "8"))
			proximity_started = true;

		if (accelerometer_started && proximity_started)
			show_gameplay_page(); // Go to the actual game page

		g_free(channel_type);
	} else if (!(strcmp(result_type, "onRead"))) { // 3. Next portion of accelerometer data received

		// Checking if the data come from Accelerometer channel
		char *channel_type = NULL;
		conv_channel_get_string(channel_h, "type", &channel_type);
		if (!strcmp(channel_type, "2")) {
			// Extracting acceleration data from the payload
			char *x = NULL;
			char *y = NULL;
			char *z = NULL;
			char *timestamp = NULL;

			conv_payload_get_string(payload_h, "x", &x);
			conv_payload_get_string(payload_h, "y", &y);
			conv_payload_get_string(payload_h, "z", &z);
			conv_payload_get_string(payload_h, "timestamp", &timestamp);

			dlog_print(DLOG_INFO, LOG_TAG, "\tACCEL <%s, %s, %s>-----", x, y, z);

			g_free(x);
			g_free(y);
			g_free(z);
			g_free(timestamp);
		} else if (!strcmp(channel_type, "8")) {
			// Extracting proximity data from the payload
			char *proximity = NULL;
			char *timestamp = NULL;

			conv_payload_get_string(payload_h, "proximity", &proximity);
			conv_payload_get_string(payload_h, "timestamp", &timestamp);

			dlog_print(DLOG_INFO, LOG_TAG, "\tPROXI");
			dlog_print(DLOG_INFO, LOG_TAG, "\tPROXI <%s>-----", proximity);
			dlog_print(DLOG_INFO, LOG_TAG, "\tPROXI");

			g_free(proximity);
			g_free(timestamp);
		} else {
			dlog_print(DLOG_INFO, LOG_TAG, "Unmatched channel type : %s", channel_type);
		}
		g_free(channel_type);

	} else if (!strcmp(result_type, "onStop")) { // 4. Service stopped
		dlog_print(DLOG_INFO, LOG_TAG, "Read request successful : %s Result", result_type);
	} else {
		// Very bad situation. It should never happen.
		// It indicates the inconsistency in sensor data schema.
		dlog_print(DLOG_ERROR, LOG_TAG, "Unexpected result type : %s", result_type);
	}

	g_free(result_type);
}

static void
rss_start_service_foreach_cb(conv_service_h service_h, void* user_data)
{
	dlog_print(DLOG_INFO, LOG_TAG, "rss_start_service_foreach_cb.");

	conv_service_e service = CONV_SERVICE_NONE;
	conv_service_get_type(service_h, &service);
	if (service != CONV_SERVICE_REMOTE_SENSOR)
		return;

	dlog_print(DLOG_INFO, LOG_TAG, "Found Remote Sensor Service. Stopping discovery...");
	conv_discovery_stop(__g_ad->conv_h);

	// To use the service we should clone its handle
	conv_service_clone(service_h, &__g_ad->selected_service_h);

	dlog_print(DLOG_INFO, LOG_TAG, "Starting the Service...");

	conv_service_set_listener_cb(__g_ad->selected_service_h, remote_sensor_listener_cb, NULL);

	conv_channel_h channel_h = NULL;
	conv_channel_create(&channel_h);
	conv_channel_set_string(channel_h, "type", "config");
	int ret = conv_service_start(__g_ad->selected_service_h, channel_h, NULL);
	if (ret != CONV_ERROR_NONE)
		dlog_print(DLOG_INFO, LOG_TAG, "conv_service_start error [%d]", ret);
	conv_channel_destroy(channel_h);
}

static void
rss_exists_service_foreach_cb(conv_service_h service_h, void* user_data)
{
	dlog_print(DLOG_INFO, LOG_TAG, "rss_exists_service_foreach_cb.");

	conv_service_e service = CONV_SERVICE_NONE;
	conv_service_get_type(service_h, &service);
	if (service == CONV_SERVICE_REMOTE_SENSOR) {
		dlog_print(DLOG_INFO, LOG_TAG, "[CONV_SERVICE_REMOTE_SENSOR]===========");
		bool *has_rss = (bool *)user_data;
		*has_rss = true;
	} else {
		dlog_print(DLOG_INFO, LOG_TAG, "[skipping service %d]=====", service);
	}
}

static void
remocon_list_selected_callback(void *data, Evas_Object *obj, void *event_info)
{
	dlog_print(DLOG_INFO, LOG_TAG, "remocon_list_selected_callback.");

	Elm_Object_Item *it = event_info;
	elm_list_item_selected_set(it, EINA_FALSE);

	__g_ad->selected_device_h = (conv_device_h) data;
	if (__g_ad->selected_device_h) {
		// Use remote device as a remote control
		conv_device_foreach_service(__g_ad->selected_device_h, rss_start_service_foreach_cb, NULL);
		// show_gameplay_page() is deffered untill the RSS is started.
	} else {
		// No remote control.
		// Go to the actual game page
		show_gameplay_page();
	}
}

static void
discovery_cb(conv_device_h device_h, conv_discovery_result_e result, void* user_data)
{
	appdata_s *ad = (appdata_s *)user_data;
	dlog_print(DLOG_INFO, LOG_TAG, "discovery_callback");

	switch (result) {
	case CONV_DISCOVERY_RESULT_ERROR:
		dlog_print(DLOG_INFO, LOG_TAG, "CONV_DISCOVERY_RESULT_ERROR");
		break;
	case CONV_DISCOVERY_RESULT_SUCCESS: {
		dlog_print(DLOG_INFO, LOG_TAG, "CONV_DISCOVERY_RESULT_SUCCESS");

		char *device_name = NULL;
		conv_device_get_property_string(device_h, CONV_DEVICE_NAME, &device_name);
		dlog_print(DLOG_INFO, LOG_TAG, "[device] %s ============", device_name);

		bool has_rss = false;
		conv_device_foreach_service(device_h, rss_exists_service_foreach_cb, (void *)(&has_rss));
		if (has_rss) {
			conv_device_h device_clone_h = NULL;
			conv_device_clone(device_h, &device_clone_h);

			// Add another device to the list of remote controls
			elm_list_item_append(ad->remocon_list, device_name, NULL, NULL,
					remocon_list_selected_callback, (const int *)device_clone_h);

			elm_list_go(ad->remocon_list); // Device provides RSS - adding to the list

			// TODO release handles on page destroy
		}

		g_free(device_name);
		break;
	}
	case CONV_DISCOVERY_RESULT_LOST:
		dlog_print(DLOG_INFO, LOG_TAG, "CONV_DISCOVERY_RESULT_LOST");
		break;
	case CONV_DISCOVERY_RESULT_FINISHED:
		dlog_print(DLOG_INFO, LOG_TAG, "CONV_DISCOVERY_RESULT_FINISHED");
		break;
	}
}

static Eina_Bool page_remocon_pop_callback(void *data, Elm_Object_Item *it)
{
	dlog_print(DLOG_INFO, LOG_TAG, "page_remocon_pop_callback.");
	ui_app_exit();
	return EINA_FALSE;
}

static void
show_remocon_page()
{
	dlog_print(DLOG_INFO, LOG_TAG, "show_home_page.");

	Evas_Object *remocon_page = elm_grid_add(__g_ad->nf);
	evas_object_show(remocon_page);

	Evas_Object *remocon_list_frame = elm_bg_add(remocon_page);
	elm_grid_pack(remocon_page, remocon_list_frame, 0, 0, 100, 100);
	evas_object_show(remocon_list_frame);

	__g_ad->remocon_list = elm_list_add(remocon_list_frame);
	elm_object_content_set(remocon_list_frame, __g_ad->remocon_list);
	elm_list_mode_set(__g_ad->remocon_list, ELM_LIST_COMPRESS);
	elm_list_item_append(__g_ad->remocon_list, "No remocon", NULL, NULL, remocon_list_selected_callback, NULL);
	elm_list_go(__g_ad->remocon_list); // Default value: no remote control

	Elm_Object_Item *nf_it = elm_naviframe_item_push(__g_ad->nf, "Select Remote Control", NULL, NULL, remocon_page, NULL);
	elm_naviframe_item_pop_cb_set(nf_it, page_remocon_pop_callback, NULL);
}

static Eina_Bool page_gameplay_pop_callback(void *data, Elm_Object_Item *it)
{
	dlog_print(DLOG_INFO, LOG_TAG, "page_remocon_pop_callback.");
	// TODO stop and destroy the service
	ui_app_exit();
	return EINA_FALSE;
}

static void
revolver_list_selected_callback(void *data, Evas_Object *obj, void *event_info)
{
	dlog_print(DLOG_INFO, LOG_TAG, "revolver_list_selected_callback.");

	// Not needed

	/*Elm_Object_Item *it = event_info;
	elm_list_item_selected_set(it, EINA_FALSE);

	int bullit = (int) data;
	if (bullit) {
		// Put the bullit here
	}*/
}

static void main_button_click_event(void *data, Evas_Object *obj, void *event_info)
{
	dlog_print(DLOG_INFO, LOG_TAG, "main_button_click_event");

	const char *btn_text = elm_object_text_get(__g_ad->main_button);
	if (!strcmp(btn_text, "Roll!!"))
		elm_object_text_set(__g_ad->main_button, "Pull the trigger...");
	else if (!strcmp(btn_text, "Pull the trigger..."))
		elm_object_text_set(__g_ad->main_button, "Play again?");
	else if (!strcmp(btn_text, "Play again?"))
		elm_object_text_set(__g_ad->main_button, "Roll!!");

	/*if(__g_ad->main_button_lock == false){

		discovery_button_lock = true;

		const char *btn_text = elm_object_text_get(discovery_button);

		if(!strcmp(btn_text, "Discovery Start")){
			discovery_thread_start();
		}

		else if(!strcmp(btn_text, "Discovery Stop")) {
			discovery_thread_stop();
		}
	}*/
}

static void
show_gameplay_page()
{
	dlog_print(DLOG_INFO, LOG_TAG, "show_gameplay_page.");

	Evas_Object *gameplay_page = elm_grid_add(__g_ad->nf);
	evas_object_show(gameplay_page);

	Evas_Object *revolver_list_frame = elm_bg_add(gameplay_page);
	elm_grid_pack(gameplay_page, revolver_list_frame, 0, 0, 100, 100);
	evas_object_show(revolver_list_frame);

	Evas_Object *revolver_list = elm_list_add(revolver_list_frame);
	elm_object_content_set(revolver_list_frame, revolver_list);
	elm_list_mode_set(revolver_list, ELM_LIST_COMPRESS);
	elm_list_item_append(revolver_list, "[ -------- ]", NULL, NULL, revolver_list_selected_callback, (const int *)0);
	elm_list_item_append(revolver_list, "[ I###D ]", NULL, NULL, revolver_list_selected_callback, (const int *)1);
	elm_list_item_append(revolver_list, "[ -------- ]", NULL, NULL, revolver_list_selected_callback, (const int *)0);
	elm_list_item_append(revolver_list, "[ -------- ]", NULL, NULL, revolver_list_selected_callback, (const int *)0);
	elm_list_item_append(revolver_list, "[ -------- ]", NULL, NULL, revolver_list_selected_callback, (const int *)0);
	elm_list_item_append(revolver_list, "[ -------- ]", NULL, NULL, revolver_list_selected_callback, (const int *)0);
	elm_list_go(revolver_list);

	__g_ad->main_button = elm_button_add(gameplay_page);
	elm_grid_pack(gameplay_page, __g_ad->main_button, 0, 85, 100, 15);
	elm_object_text_set(__g_ad->main_button, "Roll!!");
	evas_object_smart_callback_add(__g_ad->main_button, "clicked", main_button_click_event, NULL);
	evas_object_show(__g_ad->main_button);

	Elm_Object_Item *nf_it = elm_naviframe_item_push(__g_ad->nf, "Select Remote Control", NULL, NULL, gameplay_page, NULL);
	elm_naviframe_item_pop_cb_set(nf_it, page_gameplay_pop_callback, NULL);
}

static bool
app_create(void *data)
{
	/* Hook to take necessary actions before main event loop starts
		Initialize UI resources and application's data
		If this function returns true, the main loop of application starts
		If this function returns false, the application is terminated */
	appdata_s *ad = data;

	create_base_gui(ad);

	show_remocon_page(); // Select the remote control

	/* Initialize the D2D Convergence Manager */
	dlog_print(DLOG_INFO, LOG_TAG, "Creating the Convergence Manager instance...");
	int ret = conv_create(&ad->conv_h);
	if (ret != CONV_ERROR_NONE) {
		dlog_print(DLOG_DEBUG, LOG_TAG, "Creation of conv_handle failed [%d]", ret);
		return false;
	}

	dlog_print(DLOG_INFO, LOG_TAG, "Starting Discovery...");
	ret = conv_discovery_start(ad->conv_h, 15, discovery_cb, (void *)ad);
	if (ret != CONV_ERROR_NONE) {
		dlog_print(DLOG_DEBUG, LOG_TAG, "Discovery Failed [%d]", ret);
		return false;
	}

	return true;
}

static void
app_control(app_control_h app_control, void *data)
{
	/* Handle the launch request. */
}

static void
app_pause(void *data)
{
	/* Take necessary actions when application becomes invisible. */
}

static void
app_resume(void *data)
{
	/* Take necessary actions when application becomes visible. */
}

static void
app_terminate(void *data)
{
	appdata_s *ad = (appdata_s *)data;

	dlog_print(DLOG_INFO, LOG_TAG, "Destroying the Remote Sensor Service...");
	if (__g_ad->selected_service_h != NULL) {
		conv_service_destroy(ad->selected_service_h);
		__g_ad->selected_service_h = NULL;
	}

	dlog_print(DLOG_INFO, LOG_TAG, "Destroying the Convergence Manager Instance...");
	if (__g_ad->conv_h != NULL) {
		conv_destroy(ad->conv_h);
		__g_ad->conv_h = NULL;
	}
}

static void
ui_app_lang_changed(app_event_info_h event_info, void *user_data)
{
	/*APP_EVENT_LANGUAGE_CHANGED*/
	char *language;

	int ret = app_event_get_language(event_info, &language);
	if (ret != APP_ERROR_NONE) {
		dlog_print(DLOG_ERROR, LOG_TAG, "app_event_get_language() failed. Err = %d.", ret);
		return;
	}

	if (language != NULL) {
		elm_language_set(language);
		free(language);
	}
}

static void
ui_app_orient_changed(app_event_info_h event_info, void *user_data)
{
	/*APP_EVENT_DEVICE_ORIENTATION_CHANGED*/
	return;
}

static void
ui_app_region_changed(app_event_info_h event_info, void *user_data)
{
	/*APP_EVENT_REGION_FORMAT_CHANGED*/
}

static void
ui_app_low_battery(app_event_info_h event_info, void *user_data)
{
	/*APP_EVENT_LOW_BATTERY*/
}

static void
ui_app_low_memory(app_event_info_h event_info, void *user_data)
{
	/*APP_EVENT_LOW_MEMORY*/
}

int
main(int argc, char *argv[])
{
	appdata_s ad = {0,};
	__g_ad = &ad;

	ui_app_lifecycle_callback_s event_callback = {0,};
	app_event_handler_h handlers[5] = {NULL, };

	event_callback.create = app_create;
	event_callback.terminate = app_terminate;
	event_callback.pause = app_pause;
	event_callback.resume = app_resume;
	event_callback.app_control = app_control;

	ui_app_add_event_handler(&handlers[APP_EVENT_LOW_BATTERY], APP_EVENT_LOW_BATTERY, ui_app_low_battery, &ad);
	ui_app_add_event_handler(&handlers[APP_EVENT_LOW_MEMORY], APP_EVENT_LOW_MEMORY, ui_app_low_memory, &ad);
	ui_app_add_event_handler(&handlers[APP_EVENT_DEVICE_ORIENTATION_CHANGED], APP_EVENT_DEVICE_ORIENTATION_CHANGED, ui_app_orient_changed, &ad);
	ui_app_add_event_handler(&handlers[APP_EVENT_LANGUAGE_CHANGED], APP_EVENT_LANGUAGE_CHANGED, ui_app_lang_changed, &ad);
	ui_app_add_event_handler(&handlers[APP_EVENT_REGION_FORMAT_CHANGED], APP_EVENT_REGION_FORMAT_CHANGED, ui_app_region_changed, &ad);

	int ret = ui_app_main(argc, argv, &event_callback, &ad);
	if (ret != APP_ERROR_NONE)
		dlog_print(DLOG_ERROR, LOG_TAG, "app_main() is failed. err = %d", ret);

	return ret;
}
