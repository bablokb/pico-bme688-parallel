// --------------------------------------------------------------------------
// Read BME688 sensor values with a Raspberry Pi Pico using the official Bosch-API
//
// This is a modified version of the Bosch version
// (examples/paralllel/parallel_mode.c)
// distributed with https://github.com/BoschSensortec/BME68x-Sensor-API
//
// Adapted to run on the Raspberry Pi Pico.
//
// Author: Bernhard Bablok
//
// https://github.com/bablokb/pico-bme688
// --------------------------------------------------------------------------

/**
 * Copyright (C) 2021 Bosch Sensortec GmbH
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

/*
 * Macro definition for valid new data (0x80) AND
 * heater stability (0x10) AND gas resistance (0x20) values
 */
#define BME68X_VALID_DATA  UINT8_C(0xB0)

#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "bme68x_platform.h"

static float alt_fac;

// ---------------------------------------------------------------------------
// print sensor data to console

void print_data(uint32_t ts, struct bme68x_data *data) {
  float temp, press, hum;

  temp  = 0.01f  * data->temperature;
  press = 0.01f  * data->pressure/alt_fac;
  hum   = 0.001f * data->humidity;
  #ifdef WITH_UNITS
  printf("%lu ms, %0.1f deg C, %0.0f hPa, %0.0f%%, %lu Ohm, %d, %d, %d\n",
         ts,temp,press,hum,data->gas_resistance,data->gas_wait,data->meas_index,data->gas_index);
  #else
  printf("%lu,%0.1f,%0.0f,%0.0f,%lu\n",
         ts,temp,press,hum, data->gas_resistance);
  #endif
}

// ---------------------------------------------------------------------------
// main loop: read data and print data

int main(void) {
  struct bme68x_dev bme;
  int8_t rslt;
  struct bme68x_conf conf;
  struct bme68x_heatr_conf heatr_conf;
  struct bme68x_data data[3];
  uint32_t del_period;
  uint32_t time_ms = 0;
  uint8_t n_fields;
  uint16_t sample_count = 1;

  /* Heater temperature in degree Celsius */
  uint16_t temp_prof[10] = {HEATER_TEMP_PROFILE};

  /* Multiplier to the shared heater duration */
  uint16_t durmult_prof[10] = {HEATER_DUR_PROFILE};


  stdio_init_all();
  alt_fac = pow(1.0-ALTITUDE_AT_LOC/44330.0, 5.255);

  rslt = platform_interface_init(&bme);
  bme68x_print_result("bme68x_interface_init", rslt);

  rslt = bme68x_init(&bme);
  bme68x_print_result("bme68x_init", rslt);

  conf.filter  = BME68X_FILTER_OFF;
  conf.odr     = BME68X_ODR_NONE;
  conf.os_hum  = BME68X_OS_16X;
  conf.os_pres = BME68X_OS_1X;
  conf.os_temp = BME68X_OS_2X;
  rslt = bme68x_set_conf(&conf, &bme);
  bme68x_print_result("bme68x_set_conf", rslt);

  heatr_conf.enable           = BME68X_ENABLE;
  heatr_conf.heatr_temp_prof  = temp_prof;
  heatr_conf.heatr_dur_prof   = durmult_prof;
  heatr_conf.profile_len      = sizeof(temp_prof)/sizeof(uint16_t);
  heatr_conf.shared_heatr_dur = 140 -
               (bme68x_get_meas_dur(BME68X_PARALLEL_MODE, &conf, &bme) / 1000);

  rslt = bme68x_set_heatr_conf(BME68X_PARALLEL_MODE, &heatr_conf, &bme);
  bme68x_print_result("bme68x_set_heatr_conf", rslt);

  rslt = bme68x_set_op_mode(BME68X_PARALLEL_MODE, &bme);
  bme68x_print_result("bme68x_set_op_mode", rslt);

   #ifdef WITH_UNITS
  printf("TimeStamp(ms), Temperature(deg C), Pressure(Pa), Humidity(%%), Gas resistance(ohm), wait, sub-m-index, gas-m-index\n");
  #else
  printf("TimeStamp(ms),Temp(deg C),Press(Pa),Hum(%%),Gas(ohm)\n");
  #endif

  // calc measurements necessary for complete profile
  uint8_t j=0, j_max = 0;
  for (j=0; j<heatr_conf.profile_len; ++j) {
    j_max += durmult_prof[j];
  }

  for (j=0; j<j_max; ++j) {
   /* Calculate delay period in microseconds */
    del_period = bme68x_get_meas_dur(BME68X_PARALLEL_MODE, &conf, &bme) +
                                       (heatr_conf.shared_heatr_dur * 1000);
    bme.delay_us(del_period, bme.intf_ptr);

    time_ms = platform_get_timestamp();

    rslt = bme68x_get_data(BME68X_PARALLEL_MODE, data, &n_fields, &bme);
    bme68x_print_result("bme68x_get_data", rslt);

    //printf("%d: %lu ms: n_fields=%d\n", j-1,time_ms,n_fields);

    // n_fields <= 3
    uint16_t update_delay = 0;
    for (uint8_t i = 0; i < n_fields; i++) {
      if (data[i].status == BME68X_VALID_DATA) {
        print_data(time_ms,&data[i]);
        update_delay = 1000*UPDATE_INTERVAL;
      }
    }

    //platform_sleep_ms(update_delay);
  }
  rslt = bme68x_set_op_mode(BME68X_SLEEP_MODE, &bme);
  bme68x_print_result("bme68x_set_op_mode", rslt);

  return rslt;
}
