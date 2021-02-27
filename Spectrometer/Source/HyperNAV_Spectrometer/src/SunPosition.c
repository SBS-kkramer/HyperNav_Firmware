#define PI 3.141592653589793238462643383

#include "SunPosition.h"
#include "telemetry.h"

//#define TELEPRINT(x,y) sprintf(msg, "%s: %.2lf\r\n", x, y ); taskENTER_CRITICAL(); tlm_msg( msg ); taskEXIT_CRITICAL();
#define TELEPRINT(x,y)

static double radToDeg(double angleRad);
static double degToRad(double angleDeg);
static double calcGeomMeanLongSun(double t);
static double calcGeomMeanAnomalySun(double t);
static double calcEccentricityEarthOrbit(double t);
static double calcSunEqOfCenter(double t);
static double calcSunTrueLong(double t);
# if 0
static double calcSunTrueAnomaly(double t);
static double calcSunRadVector(double t);
# endif
static double calcSunApparentLong(double t);
static double calcMeanObliquityOfEcliptic(double t);
static double calcObliquityCorrection(double t);
# if 0
static double calcSunRtAscension(double t);
# endif
static double calcSunDeclination(double t);
static double calcEquationOfTime(double t);

static double calcTrueSolarTime(double t, double Longitude, double minSinceMidnight, int timezone);
static double calcHourAngle(double t, double Longitude, double minSinceMidnight, int timezone);
static double calcSolarElevationAngle(double t, double Longitude, double Latitude, double minSinceMidnight, int timezone);
static double calcApproxAtmosRefraction(double t, double Longitude, double Latitude, double minSinceMidnight, int timezone);

#if SUN_POSITION_DEBUG == 1
char msg[128];
#endif

#if SUN_POSITION_DEBUG == 2
char msg[128];
#endif

#if SUN_POSITION_DEBUG == 3
char msg[128];
#endif

//**********************************************************************

// Convert radian angle to degrees

double radToDeg(double angleRad)
{
  return (180.0 * angleRad / PI);
}

//********************************************************************

// Convert degree angle to radians

double degToRad(double angleDeg)
{
  return (PI * angleDeg / 180.0);
}


//**********************************************************************
//* Name:    calGeomMeanLongSun
//* Type:    Function
//* Purpose: calculate the Geometric Mean Longitude of the Sun
//* Arguments:
//*   t : number of Julian centuries since J2000.0
//* Return value:
//*   the Geometric Mean Longitude of the Sun in degrees
//**********************************************************************

double calcGeomMeanLongSun(double t)
{
  double L0;
  L0 = 280.46646 + t * (36000.76983 + 0.0003032 * t);
  while(L0 > 360.0)
    {
    L0 -= 360.0;
    }
  while(L0 < 0.0)
    {
    L0 += 360.0;
    }
  return L0;// in degrees
}


//**********************************************************************
//* Name:    calGeomAnomalySun
//* Type:    Function
//* Purpose: calculate the Geometric Mean Anomaly of the Sun
//* Arguments:
//*   t : number of Julian centuries since J2000.0
//* Return value:
//*   the Geometric Mean Anomaly of the Sun in degrees
//**********************************************************************

double calcGeomMeanAnomalySun(double t)
{
  double M;
  M = 357.52911 + t * (35999.05029 - 0.0001537 * t);
  return M;// in degrees
}

//**********************************************************************
//* Name:    calcEccentricityEarthOrbit
//* Type:    Function
//* Purpose: calculate the eccentricity of earth's orbit
//* Arguments:
//*   t : number of Julian centuries since J2000.0
//* Return value:
//*   the unitless eccentricity
//**********************************************************************


double calcEccentricityEarthOrbit(double t)
{
  double e;
  e = 0.016708634 - t * (0.000042037 + 0.0000001267 * t);
  return e;// unitless
}

//**********************************************************************
//* Name:    calcSunEqOfCenter
//* Type:    Function
//* Purpose: calculate the equation of center for the sun
//* Arguments:
//*   t : number of Julian centuries since J2000.0
//* Return value:
//*   in degrees
//**********************************************************************


double calcSunEqOfCenter(double t)
{
  double m, mrad, sinm, sin2m, sin3m, C;
  m = calcGeomMeanAnomalySun(t);

  mrad = degToRad(m);
  sinm = sin(mrad);
  sin2m = sin(mrad+mrad);
  sin3m = sin(mrad+mrad+mrad);

  C = sinm * (1.914602 - t * (0.004817 + 0.000014 * t)) + sin2m * (0.019993 - 0.000101 * t) + sin3m * 0.000289;
  return C;// in degrees
}

//**********************************************************************
//* Name:    calcSunTrueLong
//* Type:    Function
//* Purpose: calculate the true longitude of the sun
//* Arguments:
//*   t : number of Julian centuries since J2000.0
//* Return value:
//*   sun's true longitude in degrees
//**********************************************************************


double calcSunTrueLong(double t)
{
  double l0, c, O;
  l0 = calcGeomMeanLongSun(t);
  c = calcSunEqOfCenter(t);

  O = l0 + c;
  return O;// in degrees
}

# if 0
//**********************************************************************
//* Name:    calcSunTrueAnomaly
//* Type:    Function
//* Purpose: calculate the true anamoly of the sun
//* Arguments:
//*   t : number of Julian centuries since J2000.0
//* Return value:
//*   sun's true anamoly in degrees
//**********************************************************************

double calcSunTrueAnomaly(double t)
{
  double m, c, v;
  m = calcGeomMeanAnomalySun(t);
  c = calcSunEqOfCenter(t);

  v = m + c;
  return v;// in degrees
}
# endif

# if 0
//**********************************************************************
//* Name:    calcSunRadVector
//* Type:    Function
//* Purpose: calculate the distance to the sun in AU
//* Arguments:
//*   t : number of Julian centuries since J2000.0
//* Return value:
//*   sun radius vector in AUs
//**********************************************************************

double calcSunRadVector(double t)
{
  double v, e, R;
  v = calcSunTrueAnomaly(t);
  e = calcEccentricityEarthOrbit(t);

  R = (1.000001018 * (1 - e * e)) / (1 + e * cos(degToRad(v)));
  return R;// in AUs
}
# endif

//**********************************************************************
//* Name:    calcSunApparentLong
//* Type:    Function
//* Purpose: calculate the apparent longitude of the sun
//* Arguments:
//*   t : number of Julian centuries since J2000.0
//* Return value:
//*   sun's apparent longitude in degrees
//**********************************************************************

double calcSunApparentLong(double t)
{
  double o, omega, lambda;
  o = calcSunTrueLong(t);

  omega = 125.04 - 1934.136 * t;
  lambda = o - 0.00569 - 0.00478 * sin(degToRad(omega));
  return lambda;// in degrees
}

//**********************************************************************
//* Name:    calcMeanObliquityOfEcliptic
//* Type:    Function
//* Purpose: calculate the mean obliquity of the ecliptic
//* Arguments:
//*   t : number of Julian centuries since J2000.0
//* Return value:
//*   mean obliquity in degrees
//**********************************************************************

double calcMeanObliquityOfEcliptic(double t)
{
  double seconds, e0;
  seconds = 21.448 - t*(46.8150 + t*(0.00059 - t*(0.001813)));
  e0 = 23.0 + (26.0 + (seconds/60.0))/60.0;
  return e0;// in degrees
}

//**********************************************************************
//* Name:    calcObliquityCorrection
//* Type:    Function
//* Purpose: calculate the corrected obliquity of the ecliptic
//* Arguments:
//*   t : number of Julian centuries since J2000.0
//* Return value:
//*   corrected obliquity in degrees
//**********************************************************************

double calcObliquityCorrection(double t)
{
  double e0, omega, e;
  e0 = calcMeanObliquityOfEcliptic(t);

  omega = 125.04 - 1934.136 * t;
  e = e0 + 0.00256 * cos(degToRad(omega));
  return e;// in degrees
}

# if 0
//**********************************************************************
//* Name:    calcSunRtAscension
//* Type:    Function
//* Purpose: calculate the right ascension of the sun
//* Arguments:
//*   t : number of Julian centuries since J2000.0
//* Return value:
//*   sun's right ascension in degrees
//**********************************************************************

double calcSunRtAscension(double t)
{
  double e, lambda, tananum, tanadenom, alpha;
  e = calcObliquityCorrection(t);
  lambda = calcSunApparentLong(t);

  tananum = (cos(degToRad(e)) * sin(degToRad(lambda)));
  tanadenom = (cos(degToRad(lambda)));
  alpha = radToDeg(atan2(tananum, tanadenom));
  return alpha;// in degrees
}
# endif

//**********************************************************************
//* Name:    calcSunDeclination
//* Type:    Function
//* Purpose: calculate the declination of the sun
//* Arguments:
//*   t : number of Julian centuries since J2000.0
//* Return value:
//*   sun's declination in degrees
//**********************************************************************

double calcSunDeclination(double t)
{
  double e, lambda, sint, theta;
  e = calcObliquityCorrection(t);
  lambda = calcSunApparentLong(t);

  sint = sin(degToRad(e)) * sin(degToRad(lambda));
  theta = radToDeg(asin(sint));
  return theta;// in degrees
}

//**********************************************************************
//* Name:    calcEquationOfTime
//* Type:    Function
//* Purpose: calculate the difference between true solar time and mean
//*solar time
//* Arguments:
//*   t : number of Julian centuries since J2000.0
//* Return value:
//*   equation of time in minutes of time
//**********************************************************************

double calcEquationOfTime(double t)
{
  double epsilon, l0, e, m, y, sin2l0, sinm, cos2l0, sin4l0, sin2m, Etime;

  epsilon = calcObliquityCorrection(t);
  l0 = calcGeomMeanLongSun(t);
  e = calcEccentricityEarthOrbit(t);
  m = calcGeomMeanAnomalySun(t);

  y = tan(degToRad(epsilon)/2.0);
  y *= y;

  sin2l0 = sin(2.0 * degToRad(l0));
  sinm   = sin(degToRad(m));
  cos2l0 = cos(2.0 * degToRad(l0));
  sin4l0 = sin(4.0 * degToRad(l0));
  sin2m  = sin(2.0 * degToRad(m));

  Etime = y * sin2l0 - 2.0 * e * sinm + 4.0 * e * y * sinm * cos2l0
      - 0.5 * y * y * sin4l0 - 1.25 * e * e * sin2m;
  return radToDeg(Etime)*4.0;// in minutes of time
}

//**********************************************************************

// True Solar Time (min)

double calcTrueSolarTime(double t, double Longitude, double minSinceMidnight, int timezone)
{
  double EquationOfTime = calcEquationOfTime(t);
  return fmod( minSinceMidnight + EquationOfTime + 4*Longitude - 60*timezone, 1440 );
}

//**********************************************************************

// Hour Angle (deg)

double calcHourAngle(double t, double Longitude, double minSinceMidnight, int timezone)
{
  double TrueSolarTime = calcTrueSolarTime(t, Longitude, minSinceMidnight, timezone);

  if( TrueSolarTime/4 < 0 )
    {
    return TrueSolarTime/4 + 180;
    }
  else
    {
    return TrueSolarTime/4 - 180;
    }
}

//**********************************************************************
//* Name:    calcSolarZenithAngle
//* Type:    Function
//* Purpose: Calculates the solar Zenith Angle
//* Arguments:
//*   t : number of Julian centuries since J2000.0
//*   Longitude: Longitude of reference point (+ to E)
//*   Latitude: Latitude of reference point (+ to N)
//*   minSinceMidngiht: Minutes Since Midngiht
//* Return value:
//*   Solar Zenith Angle (deg)
//**********************************************************************

double calcSolarZenithAngle(double t,  double Longitude, double Latitude, double minSinceMidnight, int timezone)
{
  double SunDeclination = calcSunDeclination(t);
  double HourAngle = calcHourAngle(t, Longitude, minSinceMidnight, timezone);
  return radToDeg( acos( sin(degToRad(Latitude))*sin(degToRad(SunDeclination)) + cos(degToRad(Latitude))*cos(degToRad(SunDeclination))*cos(degToRad(HourAngle)) ) );
}

//**********************************************************************
//* Name:    calcCorrectedSolarElevation
//* Type:    Function
//* Purpose: Calculates solar elevation angle without atmospheric refraction correction
//* Arguments:
//*   t : number of Julian centuries since J2000.0
//*   Longitude: Longitude of reference point (+ to E)
//*   Latitude: Latitude of reference point (+ to N)
//*   minSinceMidngiht: Minutes Since Midngiht
//* Return value:
//*   Solar Elevation Angle (deg)
//**********************************************************************

double calcSolarElevationAngle(double t, double Longitude, double Latitude, double minSinceMidnight, int timezone)
{
  double SolarZenithAngle = calcSolarZenithAngle(t, Longitude, Latitude, minSinceMidnight, timezone);
  return 90 - SolarZenithAngle;
}

//**********************************************************************
//* Name:    ApproxAtmosRefraction
//* Type:    Function
//* Purpose: Calculates approximate atmospheric refraction (deg)
//* Arguments:
//*   t : number of Julian centuries since J2000.0
//*   Longitude: Longitude of reference point (+ to E)
//*   Latitude: Latitude of reference point (+ to N)
//*   minSinceMidngiht: Minutes Since Midngiht
//* Return value:
//*   Approximate atmospheric refraction (deg)
//**********************************************************************

double calcApproxAtmosRefraction(double t, double Longitude, double Latitude, double minSinceMidnight, int timezone)
{
  double SolarElevationAngle = calcSolarElevationAngle(t, Longitude, Latitude, minSinceMidnight, timezone);

  if( SolarElevationAngle > 85 )
    {
    return 0;
    }
  else if( SolarElevationAngle > 5 )
    {
    return (58.1 / tan(degToRad(SolarElevationAngle)) - 0.07 / pow(tan(degToRad(SolarElevationAngle)), 3) + 0.000086 / pow(tan(degToRad(SolarElevationAngle)), 5))/3600;
    }
  else if( SolarElevationAngle > -0.575)
    {
    return (1735 + SolarElevationAngle*( -518.2 + SolarElevationAngle*( 103.4 + SolarElevationAngle*(-12.79 + SolarElevationAngle*0.711))))/3600;
    }
  else
    {
    return -20.772 / tan(degToRad(SolarElevationAngle))/3600;
    }

}

//**********************************************************************
//* Name:    calcCorrectedSolarElevation
//* Type:    Function
//* Purpose: Calculates Corrected Solar Elevation Angle
//* Arguments:
//*   t : number of Julian centuries since J2000.0
//*   Longitude: Longitude of reference point (+ to E)
//*   Latitude: Latitude of reference point (+ to N)
//*   minSinceMidngiht: Minutes Since Midngiht
//* Return value:
//*   Solar elevation corrected for atmospheric refraction (deg)
//**********************************************************************

double calcCorrectedSolarElevation(double t, double Longitude, double Latitude, double minSinceMidnight, int timezone)
{
  double SolarElevationAngle = calcSolarElevationAngle(t, Longitude, Latitude, minSinceMidnight, timezone);
  double ApproxAtmosRefraction = calcApproxAtmosRefraction(t, Longitude, Latitude, minSinceMidnight, timezone);
  return SolarElevationAngle + ApproxAtmosRefraction;
}

//**********************************************************************
//* Name:    calcSolarAzimuthAngle
//* Type:    Function
//* Purpose: Calculates the Sun's Azimuth (deg cw from N).
//*
//* Arguments:
//*   t : number of Julian centuries since J2000.0
//*   Longitude: Longitude of reference point (+ to E)
//*   Latitude: Latitude of reference point (+ to N)
//*   minSinceMidngiht: Minutes Since Midngiht
//* Return value:
//*   The Sun's Azimuth
//**********************************************************************

double calcSolarAzimuthAngle(double t, double Longitude, double Latitude, double minSinceMidnight, int timezone)
{
  double SunDeclination = calcSunDeclination(t);
  double HourAngle = calcHourAngle(t, Longitude, minSinceMidnight, timezone);
  double SolarZenithAngle = calcSolarZenithAngle(t, Longitude, Latitude, minSinceMidnight, timezone);
#if SUN_POSITION_DEBUG == 1
  sprintf( msg, "PARAM: %.4lf, %.2lf, %.2lf, %.2lf, %d\r\n", t, Longitude, Latitude, minSinceMidnight, timezone);
  tlm_msg( msg );
  TELEPRINT("calcGeomMeanLongSun", calcGeomMeanLongSun(t));
  TELEPRINT("calcGeomMeanAnomalySun", calcGeomMeanAnomalySun(t));
  TELEPRINT("calcSunEqOfCenter", calcSunEqOfCenter(t));
  TELEPRINT("calcSunTrueLong", calcSunTrueLong(t));
  TELEPRINT("calcSunTrueAnomaly", calcSunTrueAnomaly(t));
  TELEPRINT("calcSunRadVector", calcSunRadVector(t));
#endif
#if SUN_POSITION_DEBUG == 2
  sprintf( msg, "PARAM: %.4lf, %.2lf, %.2lf, %.2lf, %d\r\n", t, Longitude, Latitude, minSinceMidnight, timezone);
  tlm_msg( msg );
  TELEPRINT("calcSunApparentLong", calcSunApparentLong(t));
  TELEPRINT("calcMeanObliquityOfEcliptic", calcMeanObliquityOfEcliptic(t));
  TELEPRINT("calcObliquityCorrection", calcObliquityCorrection(t));
  TELEPRINT("calcSunRtAscension", calcSunRtAscension(t));
  TELEPRINT("calcSunDeclination", calcSunDeclination(t));
  TELEPRINT("calcEquationOfTime", calcEquationOfTime(t));
#endif
#if SUN_POSITION_DEBUG == 3
  sprintf( msg, "PARAM: %.4lf, %.2lf, %.2lf, %.2lf, %d\r\n", t, Longitude, Latitude, minSinceMidnight, timezone);
  tlm_msg( msg );
  TELEPRINT("calcTrueSolarTime", calcTrueSolarTime(t, Longitude, minSinceMidnight, timezone));
  TELEPRINT("calcHourAngle", calcHourAngle(t, Longitude, minSinceMidnight, timezone));
  TELEPRINT("calcSolarZenithAngle", calcSolarZenithAngle(t,  Longitude, Latitude, minSinceMidnight, timezone));
  TELEPRINT("calcSolarElevationAngle", calcSolarElevationAngle(t, Longitude, Latitude, minSinceMidnight, timezone));
  TELEPRINT("calcApproxAtmosRefraction", calcApproxAtmosRefraction(t, Longitude, Latitude, minSinceMidnight, timezone));
  TELEPRINT("calcCorrectedSolarElevation", calcCorrectedSolarElevation(t, Longitude, Latitude, minSinceMidnight, timezone));
#endif

  if( HourAngle > 0)
    {
#if SUN_POSITION_DEBUG == 3
    TELEPRINT("calcSolarAzimuthAngle", fmod(radToDeg(acos(((sin(degToRad(Latitude))*cos(degToRad(SolarZenithAngle)))-sin(degToRad(SunDeclination)))/(cos(degToRad(Latitude))*sin(degToRad(SolarZenithAngle))))) + 180, 360));
#endif
    return fmod(radToDeg(acos(((sin(degToRad(Latitude))*cos(degToRad(SolarZenithAngle)))-sin(degToRad(SunDeclination)))/(cos(degToRad(Latitude))*sin(degToRad(SolarZenithAngle))))) + 180, 360);
    }
  else
    {
#if SUN_POSITION_DEBUG == 3
    TELEPRINT("calcSolarAzimuthAngle", fmod(540 - radToDeg(acos(((sin(degToRad(Latitude))*cos(degToRad(SolarZenithAngle)))-sin(degToRad(SunDeclination)))/(cos(degToRad(Latitude))*sin(degToRad(SolarZenithAngle))))), 360));
#endif
    return fmod(540 - radToDeg(acos(((sin(degToRad(Latitude))*cos(degToRad(SolarZenithAngle)))-sin(degToRad(SunDeclination)))/(cos(degToRad(Latitude))*sin(degToRad(SolarZenithAngle))))), 360);
    }
}

/*
 * Function : calcTime
 */
void calcTime( HNV_time_t* vTime, double* minutesSinceMidnight, double* julianCentury )
{
  /* Calculates the Julian Date (used algorithm found at http://www.hermetic.ch/cal_stud/jdn.htm with a slight change to account for the fraction) */
  *julianCentury = ( 1461 * ( vTime->year + 4800 +  ( vTime->month - 14 ) / 12 ) ) / 4 +
      ( 367 * ( vTime->month - 2 - 12 * ( ( vTime->month - 14 ) / 12 ) ) ) / 12 -
      ( 3 * ( ( vTime->year + 4900 + ( vTime->month - 14 ) / 12 ) / 100 ) ) / 4 +
      vTime->day - 32075 +
      ( (double) vTime->hour + (double) vTime->min / 60 + (double) vTime->sec / 3600 - (double) vTime->timeZone ) / 24 - 0.5;

  /* Calculates the number of minutes since local midnight */
  // Checked calculation FIX.
  // Original line did not account for time zone (Note that GPS gives UTC)
  // Reference: Jira Issue N20141782-16
  // ORIGINAL LINE: *minutesSinceMidnight = ( 60 * vTime->hour + vTime->min + (double) vTime->sec / 60 );
  *minutesSinceMidnight =  60 * ((vTime->hour + vTime->timeZone + 24) % 24) + vTime->min + (double) vTime->sec / 60 ;

  /* calculates the Julian Century. Used in the sun position calculation */
  *julianCentury -= 2451545;
  *julianCentury /= 36525;
}

