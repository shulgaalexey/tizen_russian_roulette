#ifndef __d2d_russian_roulette_H__
#define __d2d_russian_roulette_H__

#include <app.h>
#include <Elementary.h>
#include <system_settings.h>
#include <efl_extension.h>
#include <dlog.h>
#include <glib.h>
#include <d2d_conv_manager.h>

#ifdef  LOG_TAG
#undef  LOG_TAG
#endif
#define LOG_TAG "RUS_ROULETTE"

#if !defined(PACKAGE)
#define PACKAGE "org.example.d2d_russian_roulette"
#endif

typedef struct appdata {
	Evas_Object *win;
	Evas_Object *conform;
	Evas_Object *nf;
	Evas_Object *remocon_list;
	Evas_Object *revolver_list;
	Evas_Object *main_button;
	bool main_button_lock;
	Ecore_Timer *timer;
	int timer_ticks_remaining;
	conv_h conv_h;
	conv_device_h selected_device_h;
	conv_service_h selected_rss_service_h;
	conv_service_h selected_rac_service_h;
} appdata_s;

#endif /* __d2d_russian_roulette_H__ */
