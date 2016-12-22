#include "d2d_russian_roulette.h"

extern appdata_s *__g_ad;

extern void show_gameplay_page();
extern void on_roll_cylinder();
extern void on_pull_trigger();
extern void remocon_list_selected_callback(void *data, Evas_Object *obj, void *event_info);

static const int CONV_SERVICE_REMOTE_SENSOR = 3;

void
toggle_accelerometer(bool start)
{
	conv_channel_h channel_h = NULL;
	conv_channel_create(&channel_h);
	conv_channel_set_string(channel_h, "type", "2"); /* Pure acceleration */

	conv_payload_h payload_h = NULL;
	conv_payload_create(&payload_h);

	if (start) {
		/* Restart the accelerometer */
		conv_payload_set_string(payload_h, "periodic", "yes");
		conv_payload_set_string(payload_h, "interval", "1000");
	} else {
		/* Set accelerometer on pause */
		conv_payload_set_string(payload_h, "periodic", "no");
	}

	conv_service_read(__g_ad->selected_rss_service_h, channel_h, payload_h);

	conv_payload_destroy(payload_h);
	conv_channel_destroy(channel_h);
}

/* TODO implement toggle_proximity as well */

static void
remote_sensor_listener_cb(conv_service_h service_h, conv_channel_h channel_h,
		conv_error_e error, conv_payload_h payload_h, void* user_data)
{
	/*
	 * That clonned handle is a handle we should use.
	 * The handle, passed to the callback is temporary within the callback scope.
	 */
	conv_service_h service_clone_h = __g_ad->selected_rss_service_h;

	if (!service_clone_h || !service_h || (error != CONV_ERROR_NONE) || !payload_h)
		return; /* Too bad, arguments are invalid */

	char *result_type = NULL;
	conv_payload_get_string(payload_h, "result_type", &result_type);

	if (!(strcmp(result_type, "onStart"))) { /* 1. Service started */
		// Preparing and sending read request
		conv_payload_h payload_h = NULL;
		conv_payload_create(&payload_h);
		conv_payload_set_string(payload_h, "periodic", "yes");
		conv_payload_set_string(payload_h, "interval", "1000");

		conv_channel_set_string(channel_h, "type", "2"); /* Pure acceleration */
		conv_service_read(service_clone_h, channel_h, payload_h);

		conv_channel_set_string(channel_h, "type", "8"); /* Proximity */
		conv_service_read(service_clone_h, channel_h, payload_h);

		conv_payload_destroy(payload_h);
	} else if (!strcmp(result_type, "onSuccess")) { /* 2. Read request processed sucessfully */
		/*
		 * Switch to gameplay page only once.
		 * Don't do it on accelerometer pause/restarted response
		 */
		static bool sensors_turned_on = false;
		if (!sensors_turned_on) {
			char *channel_type = NULL;
			conv_channel_get_string(channel_h, "type", &channel_type);

			static bool accelerometer_started = false;
			static bool proximity_started = false;

			if (!strcmp(channel_type, "2"))
				accelerometer_started = true;
			if (!strcmp(channel_type, "8"))
				proximity_started = true;

			if (accelerometer_started && proximity_started) {
				sensors_turned_on = true;
				show_gameplay_page(); /* Go to the actual game page */
			}
			g_free(channel_type);
		}
	} else if (!(strcmp(result_type, "onRead"))) { /* 3. Next portion of accelerometer data received */
		/* Checking if the data come from Accelerometer channel */
		char *channel_type = NULL;
		conv_channel_get_string(channel_h, "type", &channel_type);
		if (!strcmp(channel_type, "2")) {
			/* Extracting acceleration data from the payload */
			char *x = NULL;
			char *y = NULL;
			char *z = NULL;
			conv_payload_get_string(payload_h, "x", &x);
			conv_payload_get_string(payload_h, "y", &y);
			conv_payload_get_string(payload_h, "z", &z);
			const double dx = atof(x);
			const double dy = atof(y);
			const double dz = atof(z);
			g_free(x);
			g_free(y);
			g_free(z);

			const double a = sqrt(dx * dx + dy * dy + dz * dz);
			if (a > 15.) {
				/* Roll event detected. */
				dlog_print(DLOG_INFO, LOG_TAG, "***** ROLL EVENT DETECTED! *****");
				on_roll_cylinder();
			}
		} else if (!strcmp(channel_type, "8")) {
			/* Extracting proximity data from the payload */
			char *proximity = NULL;
			conv_payload_get_string(payload_h, "proximity", &proximity);
			if (atof(proximity) == .0) { /* The trigger pulled. */
				dlog_print(DLOG_INFO, LOG_TAG, "***** TRIGGER PULLED! *****");
				on_pull_trigger();
			}
			g_free(proximity);
		}
		g_free(channel_type);
	} else {
		dlog_print(DLOG_ERROR, LOG_TAG, "Unexpected result type : %s", result_type);
	}
	g_free(result_type);
}

static void
remote_app_control_listener_cb(conv_service_h service_h, conv_channel_h channel_h,
		conv_error_e error, conv_payload_h payload_h, void* user_data)
{
	/* Empty */
	/*
	 * TODO How to terminate remotely the media player?
	 * app_control_send_terminate_request(app_control_h app_control);??
	 */
}

void
rss_start_service_foreach_cb(conv_service_h service_h, void* user_data)
{
	conv_service_e service = CONV_SERVICE_NONE;
	conv_service_get_type(service_h, &service);
	if (service == CONV_SERVICE_REMOTE_SENSOR) {
		/*
		 * To use the service we should clone its handle,
		 * because the handle passed is valid only in the callback scope.
		 */
		conv_service_clone(service_h, &__g_ad->selected_rss_service_h);

		conv_service_set_listener_cb(__g_ad->selected_rss_service_h, remote_sensor_listener_cb, NULL);

		conv_channel_h channel_h = NULL;
		conv_channel_create(&channel_h);
		conv_channel_set_string(channel_h, "type", "config");

		conv_service_start(__g_ad->selected_rss_service_h, channel_h, NULL);

		conv_channel_destroy(channel_h);
	} else if (service == CONV_SERVICE_REMOTE_APP_CONTROL) {
		conv_service_clone(service_h, &__g_ad->selected_rac_service_h);
		conv_service_set_listener_cb(__g_ad->selected_rac_service_h, remote_app_control_listener_cb, NULL);
		conv_service_start(__g_ad->selected_rac_service_h, NULL, NULL);
	}
}

static void
rss_exists_service_foreach_cb(conv_service_h service_h, void* user_data)
{
	conv_service_e service = CONV_SERVICE_NONE;
	conv_service_get_type(service_h, &service);
	if (service == CONV_SERVICE_REMOTE_SENSOR) {
		bool *has_rss = (bool *)user_data;
		*has_rss = true;
	}
}

static void
d2d_discovery_cb(conv_device_h device_h, conv_discovery_result_e result, void* user_data)
{
	switch (result) {
	case CONV_DISCOVERY_RESULT_SUCCESS: {
		char *device_name = NULL;
		conv_device_get_property_string(device_h, CONV_DEVICE_NAME, &device_name);
		dlog_print(DLOG_INFO, LOG_TAG, "[device] %s ============", device_name);

		bool has_rss = false;
		conv_device_foreach_service(device_h, rss_exists_service_foreach_cb, (void *)(&has_rss));
		if (has_rss) {
			conv_device_h device_clone_h = NULL;
			conv_device_clone(device_h, &device_clone_h);

			/* Add another device to the list of remote controls */
			elm_list_item_append(__g_ad->remocon_list, device_name, NULL, NULL,
					remocon_list_selected_callback, (const int *)device_clone_h);

			elm_list_go(__g_ad->remocon_list);

			// TODO release handles on page destroy
		}
		g_free(device_name);
		break;
	}
	case CONV_DISCOVERY_RESULT_FINISHED:
		dlog_print(DLOG_INFO, LOG_TAG, "CONV_DISCOVERY_RESULT_FINISHED");
		break;
	case CONV_DISCOVERY_RESULT_ERROR:
	case CONV_DISCOVERY_RESULT_LOST:
	default:
		break;
	}
}

void
deinit_d2d()
{
	dlog_print(DLOG_INFO, LOG_TAG, "Destroying the D2D Services and Manager");
	if (__g_ad->conv_h) {
		conv_service_destroy(__g_ad->selected_rss_service_h);
		conv_service_destroy(__g_ad->selected_rac_service_h);
		conv_destroy(__g_ad->conv_h);
	}
	__g_ad->selected_rss_service_h = NULL;
	__g_ad->selected_rac_service_h = NULL;
	__g_ad->conv_h = NULL;
}

bool
init_d2d()
{
	dlog_print(DLOG_INFO, LOG_TAG, "Creating the D2D Services and Manager");
	if (conv_create(&__g_ad->conv_h) != CONV_ERROR_NONE)
			return false;

	if (conv_discovery_start(__g_ad->conv_h, 15, d2d_discovery_cb, NULL) != CONV_ERROR_NONE)
		return false;

	return true;
}
