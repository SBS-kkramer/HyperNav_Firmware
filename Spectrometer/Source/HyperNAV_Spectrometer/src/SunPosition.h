/********************************************************************************
 *                              ** Sun Position **
 *                                 ^^^^^^^^^^^^
 *
 * Calculates the sun's azimuth and zenith based on an input time (measured in
 * Julian Centuries), longitude, latitude, and the number of minutes since
 * local midnight. The calculates are based off of JavaScript routines and an Excel
 * document provided by NOAA at the following site:
 * http://www.srrb.noaa.gov/highlights/sunrise/calcdetails.html .
 *
 ********************************************************************************/

#ifndef SUNPOSITION_H
#define SUNPOSITION_H

#include <string.h>
#include <math.h>

/* structure to hold time data from GPS */
typedef struct HNV_TIME
{
  int year;
  int month;
  int day;
  int hour;
  int min;
  int sec;
  int timeZone;

} HNV_time_t;


typedef struct GPS_DATA_T
{
//Bool valid_position;
  double lon;
  double lat;
  double heading;
//double speed;
  double magnetic_declination;

} gps_data_t;

void calcTime( HNV_time_t* vTime, double* minutesSinceMidnight, double* julianCentury );

double calcCorrectedSolarElevation(double t, double Longitude, double Latitude, double minSinceMidnight, int timezone);

/* Calculates the solar azimuth */
double calcSolarAzimuthAngle(double t, double Longitude, double Latitude, double minSinceMidnight, int timezone);

/* Calculates the solar zenith */
double calcSolarZenithAngle(double t, double Longitude, double Latitude, double minSinceMidnight, int timezone);

#endif
