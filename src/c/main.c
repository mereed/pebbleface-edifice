#include <pebble.h>
#include "main.h"
#include "hands.h"

#define ANTIALIASING true

//WINDOW
Window* window;
//LAYER
Layer* layer;

static Layer *window_layer;

//BACKGROUND
static GBitmap* background;
static BitmapLayer *background_layer; 

//SETTINGS
bool seconds = true;
bool date = true;

TextLayer *layer_date_text;
TextLayer *layer_time_text;

GBitmap *img_battery_10;
GBitmap *img_battery_9;
GBitmap *img_battery_8;
GBitmap *img_battery_7;
GBitmap *img_battery_6;
GBitmap *img_battery_5;
GBitmap *img_battery_4;
GBitmap *img_battery_3;
GBitmap *img_battery_2;
GBitmap *img_battery_1;
GBitmap *img_battery_charge;
BitmapLayer *layer_batt_img;

int charge_percent = 0;
int cur_day = -1;

static GBitmap *bluetooth_image;
static BitmapLayer *bluetooth_layer;
static GBitmap *bluetoothp_image;
static BitmapLayer *bluetoothp_layer;    

static GBitmap *time_format_image;
static BitmapLayer *time_format_layer;

#define TOTAL_DATE_DIGITS 2	
static GBitmap *date_digits_images[TOTAL_DATE_DIGITS];
static BitmapLayer *date_digits_layers[TOTAL_DATE_DIGITS];

const int DATENUM_IMAGE_RESOURCE_IDS[] = {
 RESOURCE_ID_IMAGE_DATENUM_0,
 RESOURCE_ID_IMAGE_DATENUM_1,
 RESOURCE_ID_IMAGE_DATENUM_2,
 RESOURCE_ID_IMAGE_DATENUM_3,
 RESOURCE_ID_IMAGE_DATENUM_4,
 RESOURCE_ID_IMAGE_DATENUM_5,
 RESOURCE_ID_IMAGE_DATENUM_6,
 RESOURCE_ID_IMAGE_DATENUM_7,
 RESOURCE_ID_IMAGE_DATENUM_8,
 RESOURCE_ID_IMAGE_DATENUM_9
};

static GBitmap *day_name_image;
static BitmapLayer *day_name_layer;

const int DAY_NAME_IMAGE_RESOURCE_IDS[] = {
  RESOURCE_ID_IMAGE_DAY_NAME_SUN,
  RESOURCE_ID_IMAGE_DAY_NAME_MON,
  RESOURCE_ID_IMAGE_DAY_NAME_TUE,
  RESOURCE_ID_IMAGE_DAY_NAME_WED,
  RESOURCE_ID_IMAGE_DAY_NAME_THU,
  RESOURCE_ID_IMAGE_DAY_NAME_FRI,
  RESOURCE_ID_IMAGE_DAY_NAME_SAT
};
// A struct for our specific settings (see main.h)
ClaySettings settings;

int stringToInt(char *str);

static GPath *s_minute_arrow, *s_hour_arrow, *s_second_arrow;

static Layer  *s_progress_layer, *s_average_layer;
static TextLayer  *s_step_layer;

static char s_current_steps_buffer[16];
static int s_step_count = 0, s_step_goal = 0, s_step_average = 0;

GColor color_loser;
GColor color_winner;

// Is step data available?
bool step_data_is_available() {
  return HealthServiceAccessibilityMaskAvailable &
    health_service_metric_accessible(HealthMetricStepCount,
      time_start_of_today(), time(NULL));
}

// Daily step goal
static void get_step_goal() {
  const time_t start = time_start_of_today();
  const time_t end = start + SECONDS_PER_DAY;
  s_step_goal = (int)health_service_sum_averaged(HealthMetricStepCount, start, end, HealthServiceTimeScopeDaily);
  //APP_LOG(APP_LOG_LEVEL_DEBUG,"Step goal: %d",s_step_goal);
}

// Todays current step count
static void get_step_count() {
  s_step_count = (int)health_service_sum_today(HealthMetricStepCount);
}

// Average daily step count for this time of day
static void get_step_average() {
  const time_t start = time_start_of_today();
  const time_t end = time(NULL);
  s_step_average = (int)health_service_sum_averaged(HealthMetricStepCount, start, end, HealthServiceTimeScopeDaily);
  if(s_step_average>s_step_goal)
    s_step_average=s_step_goal;
  //APP_LOG(APP_LOG_LEVEL_DEBUG,"Step average: %d",s_step_average);
}

static void display_step_count() {
  int thousands = s_step_count / 1000;
  int hundreds = s_step_count % 1000;
 // static char s_emoji[5];

  if(s_step_count >= s_step_average) {
    text_layer_set_text_color(s_step_layer, color_winner);
   // snprintf(s_emoji, sizeof(s_emoji), "\U0001F60C");
  } else {
    text_layer_set_text_color(s_step_layer, color_loser);
  //  snprintf(s_emoji, sizeof(s_emoji), "\U0001F4A9");
  }

  if(thousands > 0) {
    snprintf(s_current_steps_buffer, sizeof(s_current_steps_buffer),
      "%d,%03d", thousands, hundreds);
  } else {
    snprintf(s_current_steps_buffer, sizeof(s_current_steps_buffer),
      "%d", hundreds);
  }

  text_layer_set_text(s_step_layer, s_current_steps_buffer);
}

static void health_handler(HealthEventType event, void *context) {
  if(event == HealthEventSignificantUpdate) {
    get_step_goal();
  }

  if(event != HealthEventSleepUpdate) {
    get_step_count();
    get_step_average();
    display_step_count();
    layer_mark_dirty(s_progress_layer);
    layer_mark_dirty(s_average_layer);
  }
}

static void progress_layer_update_proc(Layer *layer, GContext *ctx) {
  const GRect inset = grect_inset(layer_get_bounds(layer), GEdgeInsets(2));
#ifdef PBL_COLOR
  graphics_context_set_fill_color(ctx,
    s_step_count >= s_step_average ? color_winner : color_loser);
#else
graphics_context_set_fill_color(ctx, GColorWhite);
#endif
  graphics_fill_radial(ctx, inset, GOvalScaleModeFitCircle, 2,
    DEG_TO_TRIGANGLE(0),
    DEG_TO_TRIGANGLE((360 * s_step_count) / s_step_goal));
}

static void average_layer_update_proc(Layer *layer, GContext *ctx) {
  if(s_step_average < 1) {
    return;
  }

  const GRect inset = grect_inset(layer_get_bounds(layer), GEdgeInsets(2));
  graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorRed,GColorWhite));

  int trigangle = DEG_TO_TRIGANGLE(360 * s_step_average / s_step_goal);
  int line_width_trigangle = 1000;
  // draw a very narrow radial (it's just a line)
  graphics_fill_radial(ctx, inset, GOvalScaleModeFitCircle, 10,
    trigangle - line_width_trigangle, trigangle);
}



// Initialize the default settings
static void prv_default_settings() {	
  settings.date = true;
  settings.secs = true;
  settings.day = false;
  settings.hrcol = GColorWhite;
  settings.mincol = GColorWhite;
  settings.seccol = GColorRed;
}

static void tick(struct tm *tick_time, TimeUnits units_changed);

static void set_container_image(GBitmap **bmp_image, BitmapLayer *bmp_layer, const int resource_id, GPoint origin) {
  GBitmap *old_image = *bmp_image;

  *bmp_image = gbitmap_create_with_resource(resource_id);
	
  GRect bounds = gbitmap_get_bounds(*bmp_image);

  GRect main_frame = GRect(origin.x, origin.y, bounds.size.w, bounds.size.h);
  bitmap_layer_set_bitmap(bmp_layer, *bmp_image);
  layer_set_frame(bitmap_layer_get_layer(bmp_layer), main_frame);

  if (old_image != NULL) {
  	gbitmap_destroy(old_image);
  }
}

static void update_days(struct tm *tick_time) {
  set_container_image(&day_name_image, day_name_layer, DAY_NAME_IMAGE_RESOURCE_IDS[tick_time->tm_wday], GPoint( 40, 50));
  set_container_image(&date_digits_images[0], date_digits_layers[0], DATENUM_IMAGE_RESOURCE_IDS[tick_time->tm_mday/10], GPoint(123,125));
  set_container_image(&date_digits_images[1], date_digits_layers[1], DATENUM_IMAGE_RESOURCE_IDS[tick_time->tm_mday%10], GPoint(129,130));

  if (!clock_is_24h_style()) {    
	  if (tick_time->tm_hour >= 12) {
		  
#ifdef PBL_PLATFORM_CHALK
      set_container_image(&time_format_image, time_format_layer, RESOURCE_ID_IMAGE_PM_MODE, GPoint(62, 126));
      layer_set_hidden(bitmap_layer_get_layer(time_format_layer), false);	
#else
      set_container_image(&time_format_image, time_format_layer, RESOURCE_ID_IMAGE_PM_MODE, GPoint(120, 44));
      layer_set_hidden(bitmap_layer_get_layer(time_format_layer), false);
#endif		
    } 
    else {
	  
#ifdef PBL_PLATFORM_CHALK
	set_container_image(&time_format_image, time_format_layer, RESOURCE_ID_IMAGE_AM_MODE, GPoint(70, 126));
    layer_set_hidden(bitmap_layer_get_layer(time_format_layer), false);			
#else
	  set_container_image(&time_format_image, time_format_layer, RESOURCE_ID_IMAGE_AM_MODE, GPoint(120, 44));
      layer_set_hidden(bitmap_layer_get_layer(time_format_layer), false);
#endif	
    }
  }
}


void update_layer(Layer *me, GContext* ctx) 
{
//watchface drawing
		
	//get tick_time
	time_t temp = time(NULL); 
  	struct tm *tick_time = localtime(&temp);
	
	
	//draw hands
#ifdef PBL_PLATFORM_CHALK
	GPoint center = GPoint(90,90);
#else
	GPoint center = GPoint(72,84);
#endif

	if((settings.secs) && (seconds == 1)) {
	
		tick_timer_service_subscribe(SECOND_UNIT, tick);
		gpath_rotate_to(s_second_arrow, TRIG_MAX_ANGLE * tick_time->tm_sec / 60);
		graphics_context_set_antialiased(ctx, ANTIALIASING);
		graphics_context_set_stroke_width(ctx, 2);
		graphics_context_set_stroke_color(ctx, settings.seccol);
		gpath_draw_outline(ctx, s_second_arrow);
				
	} else {
		tick_timer_service_subscribe(MINUTE_UNIT, tick);
	}		
	
  graphics_context_set_stroke_width(ctx, 2);
  graphics_context_set_antialiased(ctx, ANTIALIASING);

	
	// minute hand
	graphics_context_set_stroke_color(ctx, settings.mincol);
	gpath_rotate_to(s_minute_arrow, TRIG_MAX_ANGLE * tick_time->tm_min / 60);
	gpath_draw_outline(ctx, s_minute_arrow);

	// hour hand
	graphics_context_set_stroke_color(ctx, settings.hrcol);
	gpath_rotate_to(s_hour_arrow, (TRIG_MAX_ANGLE * (((tick_time->tm_hour % 12) * 6) + (tick_time->tm_min / 10))) / (12 * 6)); // from Pebble SDK example
	gpath_draw_outline(ctx, s_hour_arrow);
	
	
    graphics_context_set_fill_color(ctx, GColorBlack);
	graphics_fill_circle(ctx, center, 6);
	graphics_context_set_fill_color(ctx, GColorWhite);
	graphics_fill_circle(ctx, center, 4);

}

// Read settings from persistent storage
static void prv_load_settings() {
  // Load the default settings
  prv_default_settings();
  // Read settings from persistent storage, if they exist
  persist_read_data(SETTINGS_KEY, &settings, sizeof(settings));
}

// Save the settings to persistent storage
static void prv_save_settings() {
  persist_write_data(SETTINGS_KEY, &settings, sizeof(settings));
  // Update the display based on new settings
  prv_update_display();
}


// Update the display elements
static void prv_update_display() {
	
	if (settings.date){
	  for (int i = 0; i < TOTAL_DATE_DIGITS; ++i) {
		  layer_set_hidden(bitmap_layer_get_layer(date_digits_layers[i]), false);
	  }
	} else {
	  for (int i = 0; i < TOTAL_DATE_DIGITS; ++i) {
		  layer_set_hidden(bitmap_layer_get_layer(date_digits_layers[i]), true);
	  }
	}
	
	if (settings.day){
		  layer_set_hidden(bitmap_layer_get_layer(day_name_layer), false);
	} else {
		  layer_set_hidden(bitmap_layer_get_layer(day_name_layer), true);		
	}
}

int stringToInt(char *str){
    int i=0,sum=0;
    while(str[i]!='\0'){
         if(str[i]< 48 || str[i] > 57){
            // if (DEBUG) APP_LOG(APP_LOG_LEVEL_ERROR, "Unable to convert it into integer.");
          //   return 0;
         }
         else{
             sum = sum*10 + (str[i] - 48);
             i++;
         }
    }
    return sum;
}

// Handle the response from AppMessage
static void prv_inbox_received_handler(DictionaryIterator *iter, void *context) {
	
  // background
  Tuple *bg_t = dict_find(iter, MESSAGE_KEY_background);
  if (bg_t) {
    settings.background = stringToInt((char*) bg_t->value->data);
  }

 // day
  Tuple *day_t = dict_find(iter, MESSAGE_KEY_day);
  if (day_t) {
    settings.day = day_t->value->int32 == 1;
  }
	
 // date
  Tuple *date_t = dict_find(iter, MESSAGE_KEY_date);
  if (date_t) {
    settings.date = date_t->value->int32 == 1;
  }
	
// secs
  Tuple *secs_t = dict_find(iter, MESSAGE_KEY_secs);
  if (secs_t) {
    settings.secs = secs_t->value->int32 == 1;
  }

 // batt
  Tuple *batt_t = dict_find(iter, MESSAGE_KEY_batt);
  if (batt_t) {
    settings.batt = batt_t->value->int32 == 1;
  }
	
 // Hour hand Color
  Tuple *hr_color_t = dict_find(iter, MESSAGE_KEY_hrcol);
  if (hr_color_t) {
    settings.hrcol = GColorFromHEX(hr_color_t->value->int32);
  }
	
 // Minute hand Color
  Tuple *min_color_t = dict_find(iter, MESSAGE_KEY_mincol);
  if (min_color_t) {
    settings.mincol = GColorFromHEX(min_color_t->value->int32);
  }
	
 // Second hand Color
  Tuple *sec_color_t = dict_find(iter, MESSAGE_KEY_seccol);
  if (sec_color_t) {
    settings.seccol = GColorFromHEX(sec_color_t->value->int32);
  }

// Save the new settings to persistent storage
  prv_save_settings();
}

void update_battery_state(BatteryChargeState charge_state) {

    if (charge_state.is_charging) {
        bitmap_layer_set_bitmap(layer_batt_img, img_battery_charge);
		
    } else {

        if (charge_state.charge_percent <= 10) {
            bitmap_layer_set_bitmap(layer_batt_img, img_battery_1);
        } else if (charge_state.charge_percent <= 20) {
            bitmap_layer_set_bitmap(layer_batt_img, img_battery_2);
		} else if (charge_state.charge_percent <= 30) {
            bitmap_layer_set_bitmap(layer_batt_img, img_battery_3);
		} else if (charge_state.charge_percent <= 40) {
            bitmap_layer_set_bitmap(layer_batt_img, img_battery_4);
		} else if (charge_state.charge_percent <= 50) {
            bitmap_layer_set_bitmap(layer_batt_img, img_battery_5);
		} else if (charge_state.charge_percent <= 60) {
            bitmap_layer_set_bitmap(layer_batt_img, img_battery_6);
		} else if (charge_state.charge_percent <= 70) {
            bitmap_layer_set_bitmap(layer_batt_img, img_battery_7);
		} else if (charge_state.charge_percent <= 80) {
            bitmap_layer_set_bitmap(layer_batt_img, img_battery_8);	
		} else if (charge_state.charge_percent <= 90) {
            bitmap_layer_set_bitmap(layer_batt_img, img_battery_9);	
		} else if (charge_state.charge_percent <= 100) {
            bitmap_layer_set_bitmap(layer_batt_img, img_battery_10);

		} else {
            bitmap_layer_set_bitmap(layer_batt_img, img_battery_10);
        } 
    } 
    charge_percent = charge_state.charge_percent; 
} 

static void toggle_bluetooth_icon(bool connected) {

  layer_set_hidden(bitmap_layer_get_layer(bluetooth_layer), !connected);
  layer_set_hidden(bitmap_layer_get_layer(bluetoothp_layer), connected);
	  
}

void bluetooth_connection_callback(bool connected) {
  toggle_bluetooth_icon(connected);
}

void tick(struct tm *tick_time, TimeUnits units_changed) {	

	//redraw every tick
	layer_mark_dirty(layer);
	
	 if (units_changed & HOUR_UNIT) {
    update_days(tick_time);
  }
}


void init() {
	
  prv_load_settings();

  color_loser = GColorRed;
  color_winner = GColorWhite;

  // Listen for AppMessages
  app_message_register_inbox_received(prv_inbox_received_handler);
  app_message_open(128, 128);

  // international support
  setlocale(LC_ALL, "");
	
	//create window
	window = window_create();
	window_set_background_color(window,GColorBlack);
	window_stack_push(window, true);
	Layer* window_layer = window_get_root_layer(window);	
	GRect bounds = layer_get_bounds(window_layer);

  //background
  background = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BACKGROUND);
  background_layer = bitmap_layer_create(bounds);
  bitmap_layer_set_bitmap(background_layer, background);
  layer_add_child(window_layer, bitmap_layer_get_layer(background_layer));
	
	
  img_battery_10   = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATT_10);
  img_battery_9    = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATT_9);
  img_battery_8    = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATT_8);
  img_battery_7    = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATT_7);
  img_battery_6    = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATT_6);
  img_battery_5    = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATT_5);
  img_battery_4    = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATT_4);
  img_battery_3    = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATT_3);
  img_battery_2    = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATT_2);
  img_battery_1    = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATT_1);
  img_battery_charge = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATT_CHARGING);
		
#if PBL_PLATFORM_CHALK	
  layer_batt_img  = bitmap_layer_create(GRect(88, 87, 64, 64));
#else
  layer_batt_img  = bitmap_layer_create(GRect(59, 134, 25, 24));
#endif
  bitmap_layer_set_bitmap(layer_batt_img, img_battery_3);
  bitmap_layer_set_compositing_mode(layer_batt_img, GCompOpSet);
  layer_add_child(window_layer, bitmap_layer_get_layer(layer_batt_img));
	
  bluetooth_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BLUETOOTH);
  GRect bitmap_bounds_bt_on = gbitmap_get_bounds(bluetooth_image);
#if PBL_PLATFORM_CHALK	
  GRect frame_bt = GRect(42, 31, bitmap_bounds_bt_on.size.w, bitmap_bounds_bt_on.size.h);
#else
  GRect frame_bt = GRect(68, 142, bitmap_bounds_bt_on.size.w, bitmap_bounds_bt_on.size.h);
#endif
  bluetooth_layer = bitmap_layer_create(frame_bt);
  bitmap_layer_set_bitmap(bluetooth_layer, bluetooth_image);
  layer_add_child(window_layer, bitmap_layer_get_layer(bluetooth_layer));	
		
  bluetoothp_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_PLANE);
  GRect bitmap_bounds_btp_on = gbitmap_get_bounds(bluetoothp_image);
#if PBL_PLATFORM_CHALK	
  GRect frame_btp = GRect(129, 38, bitmap_bounds_btp_on.size.w, bitmap_bounds_btp_on.size.h);
#else
  GRect frame_btp = GRect(68, 142, bitmap_bounds_btp_on.size.w, bitmap_bounds_btp_on.size.h);
#endif
  bluetoothp_layer = bitmap_layer_create(frame_btp);
  bitmap_layer_set_bitmap(bluetoothp_layer, bluetoothp_image);
  layer_add_child(window_layer, bitmap_layer_get_layer(bluetoothp_layer));
 // layer_set_hidden(bitmap_layer_get_layer(bluetoothp_layer), true);
	
  GRect dummy_frame = { {0, 0}, {0, 0} };	
  day_name_layer = bitmap_layer_create(dummy_frame);
  bitmap_layer_set_compositing_mode(day_name_layer, GCompOpSet);
  layer_add_child(window_layer, bitmap_layer_get_layer(day_name_layer));	
	
	
  for (int i = 0; i < TOTAL_DATE_DIGITS; ++i) {
    date_digits_layers[i] = bitmap_layer_create(dummy_frame);
	GCompOp compositing_mode = GCompOpSet;
    bitmap_layer_set_compositing_mode(date_digits_layers[i], compositing_mode);
    layer_add_child(window_layer, bitmap_layer_get_layer(date_digits_layers[i]));
  }

#ifdef PBL_PLATFORM_CHALK
  GRect frame5 = GRect(45, 45, 3,5);
#else
  GRect frame5 = GRect(0, 0, 3,5);
#endif
  time_format_layer = bitmap_layer_create(frame5);
  layer_add_child(window_layer, bitmap_layer_get_layer(time_format_layer));
	
	


  // Steps Progress indicator
  s_progress_layer = layer_create(GRect(29,64,53,53));
  layer_set_update_proc(s_progress_layer, progress_layer_update_proc);
  layer_add_child(window_layer, s_progress_layer);

  // Steps Average indicator
  s_average_layer = layer_create(GRect(29,64,53,53));
  layer_set_update_proc(s_average_layer, average_layer_update_proc);
  layer_add_child(window_layer, s_average_layer);


  // Create a layer to hold the current step count text
  s_step_layer = text_layer_create(
      GRect(0, 43, 180, 20));
  text_layer_set_background_color(s_step_layer, GColorClear);
  text_layer_set_text_color(s_step_layer, GColorWhite);
  text_layer_set_font(s_step_layer,fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD));
  text_layer_set_text_alignment(s_step_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_step_layer));

	//create hands layer
#if PBL_PLATFORM_CHALK	
	layer = layer_create(GRect(0,0,180,180));
#else
	layer = layer_create(GRect(0,0,144,168));
#endif
	layer_set_update_proc(layer, update_layer);
	layer_add_child(window_layer, layer);	
	
	
	prv_update_display();

	// Subscribe to health events if we can
  if(step_data_is_available()) {
    health_service_events_subscribe(health_handler, NULL);
  }
	
	
  toggle_bluetooth_icon(bluetooth_connection_service_peek());
  update_battery_state(battery_state_service_peek());

  //subscribe to seconds tick event
  tick_timer_service_subscribe(MINUTE_UNIT, tick);
	bluetooth_connection_service_subscribe(bluetooth_connection_callback);
    battery_state_service_subscribe(&update_battery_state);
	
// init hand paths
  s_second_arrow = gpath_create(&SECOND_HAND_POINTS);
  s_minute_arrow = gpath_create(&MINUTE_HAND_POINTS);
  s_hour_arrow = gpath_create(&HOUR_HAND_POINTS);
  GPoint center = grect_center_point(&bounds);
  gpath_move_to(s_second_arrow, center);
  gpath_move_to(s_minute_arrow, center);
  gpath_move_to(s_hour_arrow, center);
	
	
	// Avoids a blank screen on watch start.
  time_t now = time(NULL);
  struct tm *tick_time = localtime(&now);  
  tick(tick_time, DAY_UNIT + HOUR_UNIT + MINUTE_UNIT);
	
    update_battery_state(battery_state_service_peek());
    toggle_bluetooth_icon(bluetooth_connection_service_peek());

}

static void deinit(void) {

	layer_destroy(layer);
	
	bluetooth_connection_service_unsubscribe();
    battery_state_service_unsubscribe();
	
  layer_remove_from_parent(bitmap_layer_get_layer(background_layer));
  bitmap_layer_destroy(background_layer);

	if (background != NULL) {
		gbitmap_destroy(background);
    }
	
	layer_remove_from_parent(bitmap_layer_get_layer(bluetooth_layer));
    bitmap_layer_destroy(bluetooth_layer);
    gbitmap_destroy(bluetooth_image);
    bluetooth_image = NULL;
	
  layer_remove_from_parent(bitmap_layer_get_layer(day_name_layer));
  bitmap_layer_destroy(day_name_layer);
  gbitmap_destroy(day_name_image);
		
  layer_remove_from_parent(bitmap_layer_get_layer(layer_batt_img));
  bitmap_layer_destroy(layer_batt_img);
  gbitmap_destroy(img_battery_1);
  gbitmap_destroy(img_battery_2);
  gbitmap_destroy(img_battery_3);
  gbitmap_destroy(img_battery_4);
  gbitmap_destroy(img_battery_5);
  gbitmap_destroy(img_battery_6);
  gbitmap_destroy(img_battery_7);
  gbitmap_destroy(img_battery_8);
  gbitmap_destroy(img_battery_9);
  gbitmap_destroy(img_battery_10);
  gbitmap_destroy(img_battery_charge);	
	
 for (int i = 0; i < TOTAL_DATE_DIGITS; i++) {
    layer_remove_from_parent(bitmap_layer_get_layer(date_digits_layers[i]));
    gbitmap_destroy(date_digits_images[i]);
    bitmap_layer_destroy(date_digits_layers[i]);
  }
	
	window_destroy(window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}