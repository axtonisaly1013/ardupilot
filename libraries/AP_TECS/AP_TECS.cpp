#include "AP_TECS.h"
#include <AP_HAL/AP_HAL.h>

extern const AP_HAL::HAL& hal;

#if CONFIG_HAL_BOARD == HAL_BOARD_SITL
#include <stdio.h>
# define Debug(fmt, args ...)  do {printf("%s:%d: " fmt "\n", __FUNCTION__, __LINE__, ## args); hal.scheduler->delay(1); } while(0)
#else
# define Debug(fmt, args ...)
#endif
//Debug("%.2f %.2f %.2f %.2f \n", var1, var2, var3, var4);

// table of user settable parameters
const AP_Param::GroupInfo AP_TECS::var_info[] = {

    // @Param: CLMB_MAX
    // @DisplayName: Maximum Climb Rate (metres/sec)
    // @Description: This is the best climb rate that the aircraft can achieve with the throttle set to THR_MAX and the airspeed set to the default value. For electric aircraft make sure this number can be achieved towards the end of flight when the battery voltage has reduced. The setting of this parameter can be checked by commanding a positive altitude change of 100m in loiter, RTL or guided mode. If the throttle required to climb is close to THR_MAX and the aircraft is maintaining airspeed, then this parameter is set correctly. If the airspeed starts to reduce, then the parameter is set to high, and if the throttle demand require to climb and maintain speed is noticeably less than THR_MAX, then either CLMB_MAX should be increased or THR_MAX reduced.
    // @Increment: 0.1
    // @Range: 0.1 20.0
    // @User: Standard
    AP_GROUPINFO("CLMB_MAX",    0, AP_TECS, _maxClimbRate, 5.0f),

    // @Param: SINK_MIN
    // @DisplayName: Minimum Sink Rate (metres/sec)
    // @Description: This is the sink rate of the aircraft with the throttle set to THR_MIN and the same airspeed as used to measure CLMB_MAX.
    // @Increment: 0.1
    // @Range: 0.1 10.0
    // @User: Standard
    AP_GROUPINFO("SINK_MIN",    1, AP_TECS, _minSinkRate, 2.0f),

    // @Param: TIME_CONST
    // @DisplayName: Controller time constant (sec)
    // @Description: This is the time constant of the TECS control algorithm. Smaller values make it faster to respond, large values make it slower to respond.
    // @Range: 3.0 10.0
    // @Increment: 0.2
    // @User: Advanced
    AP_GROUPINFO("TIME_CONST",  2, AP_TECS, _timeConst, 5.0f),

    // @Param: THR_DAMP
    // @DisplayName: Controller throttle damping
    // @Description: This is the damping gain for the throttle demand loop. Increase to add damping  to correct for oscillations in speed and height.
    // @Range: 0.1 1.0
    // @Increment: 0.1
    // @User: Advanced
    AP_GROUPINFO("THR_DAMP",    3, AP_TECS, _thrDamp, 0.5f),

    // @Param: INTEG_GAIN
    // @DisplayName: Controller integrator
    // @Description: This is the integrator gain on the control loop. Increase to increase the rate at which speed and height offsets are trimmed out
    // @Range: 0.0 0.5
    // @Increment: 0.02
    // @User: Advanced
    AP_GROUPINFO("INTEG_GAIN", 4, AP_TECS, _integGain, 0.1f),

    // @Param: VERT_ACC
    // @DisplayName: Vertical Acceleration Limit (metres/sec^2)
    // @Description: This is the maximum vertical acceleration either up or down that the  controller will use to correct speed or height errors.
    // @Range: 1.0 10.0
    // @Increment: 0.5
    // @User: Advanced
    AP_GROUPINFO("VERT_ACC",  5, AP_TECS, _vertAccLim, 7.0f),

    // @Param: HGT_OMEGA
    // @DisplayName: Height complementary filter frequency (radians/sec)
    // @Description: This is the cross-over frequency of the complementary filter used to fuse vertical acceleration and baro alt to obtain an estimate of height rate and height.
    // @Range: 1.0 5.0
    // @Increment: 0.05
    // @User: Advanced
    AP_GROUPINFO("HGT_OMEGA", 6, AP_TECS, _hgtCompFiltOmega, 3.0f),

    // @Param: SPD_OMEGA
    // @DisplayName: Speed complementary filter frequency (radians/sec)
    // @Description: This is the cross-over frequency of the complementary filter used to fuse longitudinal acceleration and airspeed to obtain a lower noise and lag estimate of airspeed.
    // @Range: 0.5 2.0
    // @Increment: 0.05
    // @User: Advanced
    AP_GROUPINFO("SPD_OMEGA", 7, AP_TECS, _spdCompFiltOmega, 2.0f),

    // @Param: RLL2THR
    // @DisplayName: Bank angle compensation gain
    // @Description: Increasing this gain turn increases the amount of throttle that will be used to compensate for the additional drag created by turning. Ideally this should be set to approximately 10 x the extra sink rate in m/s created by a 45 degree bank turn. Increase this gain if the aircraft initially loses energy in turns and reduce if the aircraft initially gains energy in turns. Efficient high aspect-ratio aircraft (eg powered sailplanes) can use a lower value, whereas inefficient low aspect-ratio models (eg delta wings) can use a higher value.
    // @Range: 5.0 30.0
    // @Increment: 1.0
    // @User: Advanced
    AP_GROUPINFO("RLL2THR",  8, AP_TECS, _rollComp, 10.0f),

    // @Param: SPDWEIGHT
    // @DisplayName: Weighting applied to speed control
    // @Description: This parameter adjusts the amount of weighting that the pitch control applies to speed vs height errors. Setting it to 0.0 will cause the pitch control to control height and ignore speed errors. This will normally improve height accuracy but give larger airspeed errors. Setting it to 2.0 will cause the pitch control loop to control speed and ignore height errors. This will normally reduce airsped errors, but give larger height errors.	A value of 1.0 gives a balanced response and is the default.
    // @Range: 0.0 2.0
    // @Increment: 0.1
    // @User: Advanced
    AP_GROUPINFO("SPDWEIGHT", 9, AP_TECS, _spdWeight, 1.0f),

    // @Param: PTCH_DAMP
    // @DisplayName: Controller pitch damping
    // @Description: This is the damping gain for the pitch demand loop. Increase to add damping  to correct for oscillations in speed and height.
    // @Range: 0.1 1.0
    // @Increment: 0.1
    // @User: Advanced
    AP_GROUPINFO("PTCH_DAMP", 10, AP_TECS, _ptchDamp, 0.0f),

    // @Param: SINK_MAX
    // @DisplayName: Maximum Descent Rate (metres/sec)
    // @Description: This sets the maximum descent rate that the controller will use.  If this value is too large, the aircraft will reach the pitch angle limit first and be unable to achieve the descent rate. This should be set to a value that can be achieved at the lower pitch angle limit.
    // @Increment: 0.1
    // @Range: 0.0 20.0
    // @User: User
    AP_GROUPINFO("SINK_MAX",  11, AP_TECS, _maxSinkRate, 5.0f),

//    // @Param: LAND_ARSPD
//    // @DisplayName: Airspeed during landing approach (m/s)
//    // @Description: When performing an autonomus landing, this value is used as the goal airspeed during approach.  Note that this parameter is not useful if your platform does not have an airspeed sensor (use TECS_LAND_THR instead).  If negative then this value is not used during landing.
//    // @Range: -1 127
//    // @Increment: 1
//    // @User: User
//    AP_GROUPINFO("LAND_ARSPD", 12, AP_TECS, _landAirspeed, -1),

//    // @Param: LAND_THR
//    // @DisplayName: Cruise throttle during landing approach (percentage)
//    // @Description: Use this parameter instead of LAND_ARSPD if your platform does not have an airspeed sensor.  It is the cruise throttle during landing approach.  If this value is negative then it is disabled and TECS_LAND_ARSPD is used instead.
//    // @Range: -1 100
//    // @Increment: 0.1
//    // @User: User
//    AP_GROUPINFO("LAND_THR", 13, AP_TECS, _landThrottle, -1),

//    // @Param: LAND_SPDWGT
//    // @DisplayName: Weighting applied to speed control during landing.
//    // @Description: Same as SPDWEIGHT parameter, with the exception that this parameter is applied during landing flight stages.  A value closer to 2 will result in the plane ignoring height error during landing and our experience has been that the plane will therefore keep the nose up -- sometimes good for a glider landing (with the side effect that you will likely glide a ways past the landing point).  A value closer to 0 results in the plane ignoring speed error -- use caution when lowering the value below 1 -- ignoring speed could result in a stall. Values between 0 and 2 are valid values for a fixed landing weight. When using -1 the weight will be scaled during the landing. At the start of the landing approach it starts with TECS_SPDWEIGHT and scales down to 0 by the time you reach the land point. Example: Halfway down the landing approach you'll effectively have a weight of TECS_SPDWEIGHT/2.
//    // @Range: -1.0 2.0
//    // @Increment: 0.1
//    // @User: Advanced
//    AP_GROUPINFO("LAND_SPDWGT", 14, AP_TECS, _spdWeightLand, -1.0f),

    // @Param: PITCH_MAX
    // @DisplayName: Maximum pitch in auto flight
    // @Description: This controls maximum pitch up in automatic throttle modes. If this is set to zero then LIM_PITCH_MAX is used instead. The purpose of this parameter is to allow the use of a smaller pitch range when in automatic flight than what is used in FBWA mode.
    // @Range: 0 45
    // @Increment: 1
    // @User: Advanced
    AP_GROUPINFO("PITCH_MAX", 15, AP_TECS, _pitch_max, 0),

    // @Param: PITCH_MIN
    // @DisplayName: Minimum pitch in auto flight
    // @Description: This controls minimum pitch in automatic throttle modes. If this is set to zero then LIM_PITCH_MIN is used instead. The purpose of this parameter is to allow the use of a smaller pitch range when in automatic flight than what is used in FBWA mode. Note that TECS_PITCH_MIN should be a negative number.
    // @Range: -45 0
    // @Increment: 1
    // @User: Advanced
    AP_GROUPINFO("PITCH_MIN", 16, AP_TECS, _pitch_min, 0),

//    // @Param: LAND_SINK
//    // @DisplayName: Sink rate for final landing stage
//    // @Description: The sink rate in meters/second for the final stage of landing.
//    // @Range: 0.0 2.0
//    // @Increment: 0.1
//    // @User: Advanced
//    AP_GROUPINFO("LAND_SINK", 17, AP_TECS, _land_sink, 0.25f),

//    // @Param: LAND_TCONST
//    // @DisplayName: Land controller time constant (sec)
//    // @Description: This is the time constant of the TECS control algorithm when in final landing stage of flight. It should be smaller than TECS_TIME_CONST to allow for faster flare
//    // @Range: 1.0 5.0
//    // @Increment: 0.2
//    // @User: Advanced
//    AP_GROUPINFO("LAND_TCONST", 18, AP_TECS, _landTimeConst, 2.0f),

//    // @Param: LAND_DAMP
//    // @DisplayName: Controller sink rate to pitch gain during flare
//    // @Description: This is the sink rate gain for the pitch demand loop when in final landing stage of flight. It should be larger than TECS_PTCH_DAMP to allow for better sink rate control during flare.
//    // @Range: 0.1 1.0
//    // @Increment: 0.1
//    // @User: Advanced
//    AP_GROUPINFO("LAND_DAMP", 19, AP_TECS, _landDamp, 0.5f),

//    // @Param: LAND_PMAX
//    // @DisplayName: Maximum pitch during final stage of landing
//    // @Description: This limits the pitch used during the final stage of automatic landing. During the final landing stage most planes need to keep their pitch small to avoid stalling. A maximum of 10 degrees is usually good. A value of zero means to use the normal pitch limits.
//    // @Range: -5 40
//    // @Increment: 1
//    // @User: Advanced
//    AP_GROUPINFO("LAND_PMAX", 20, AP_TECS, _land_pitch_max, 10),

//    // @Param: APPR_SMAX
//    // @DisplayName: Sink rate max for landing approach stage
//    // @Description: The sink rate max for the landing approach stage of landing. This will need to be large for steep landing approaches especially when using reverse thrust. If 0, then use TECS_SINK_MAX.
//    // @Range: 0.0 20.0
//    // @Units: m/s
//    // @Increment: 0.1
//    // @User: Advanced
//    AP_GROUPINFO("APPR_SMAX", 21, AP_TECS, _maxSinkRate_approach, 0),

//    // @Param: LAND_SRC
//    // @DisplayName: Land sink rate change
//    // @Description: When zero, the flare sink rate (TECS_LAND_SINK) is a fixed sink demand. With this enabled the flare sinkrate will increase/decrease the flare sink demand as you get further beyond the LAND waypoint. Has no effect before the waypoint. This value is added to TECS_LAND_SINK proportional to distance traveled after wp. With an increasing sink rate you can still land in a given distance if you're traveling too fast and cruise passed the land point. A positive value will force the plane to land sooner proportional to distance passed land point. A negative number will tell the plane to slowly climb allowing for a pitched-up stall landing. Recommend 0.2 as initial value.
//    // @Range: -2.0 2.0
//    // @Units: m/s/m
//    // @Increment: 0.1
//    // @User: Advanced
//    AP_GROUPINFO("LAND_SRC", 22, AP_TECS, _land_sink_rate_change, 0),

//    // @Param: LAND_TDAMP
//    // @DisplayName: Controller throttle damping when landing
//    // @Description: This is the damping gain for the throttle demand loop during and auto-landing. Same as TECS_THR_DAMP but only in effect during an auto-land. Increase to add damping to correct for oscillations in speed and height. When set to 0 landing throttle damp is controlled by TECS_THR_DAMP.
//    // @Range: 0.1 1.0
//    // @Increment: 0.1
//    // @User: Advanced
//    AP_GROUPINFO("LAND_TDAMP", 23, AP_TECS, _land_throttle_damp, 0),

//    // @Param: LAND_IGAIN
//    // @DisplayName: Controller integrator during landing
//    // @Description: This is the integrator gain on the control loop during landing. When set to 0 then TECS_INTEG_GAIN is used. Increase to increase the rate at which speed and height offsets are trimmed out. Typically values lower than TECS_INTEG_GAIN work best
//    // @Range: 0.0 0.5
//    // @Increment: 0.02
//    // @User: Advanced
//    AP_GROUPINFO("LAND_IGAIN", 24, AP_TECS, _integGain_land, 0),

//    // @Param: TKOFF_IGAIN
//    // @DisplayName: Controller integrator during takeoff
//    // @Description: This is the integrator gain on the control loop during takeoff. When set to 0 then TECS_INTEG_GAIN is used. Increase to increase the rate at which speed and height offsets are trimmed out. Typically values higher than TECS_INTEG_GAIN work best
//    // @Range: 0.0 0.5
//    // @Increment: 0.02
//    // @User: Advanced
//    AP_GROUPINFO("TKOFF_IGAIN", 25, AP_TECS, _integGain_takeoff, 0),

//    // @Param: LAND_PDAMP
//    // @DisplayName: Pitch damping gain when landing
//    // @Description: This is the damping gain for the pitch demand loop during landing. Increase to add damping  to correct for oscillations in speed and height. If set to 0 then TECS_PTCH_DAMP will be used instead.
//    // @Range: 0.1 1.0
//    // @Increment: 0.1
//    // @User: Advanced
//    AP_GROUPINFO("LAND_PDAMP", 26, AP_TECS, _land_pitch_damp, 0),

    // @Param: SYNAIRSPEED
    // @DisplayName: Enable the use of synthetic airspeed
    // @Description: This enable the use of synthetic airspeed for aircraft that don't have a real airspeed sensor. This is useful for development testing where the user is aware of the considerable limitations of the synthetic airspeed system, such as very poor estimates when a wind estimate is not accurate. Do not enable this option unless you fully understand the limitations of a synthetic airspeed estimate.
    // @Values: 0:Disable,1:Enable
    // @User: Advanced
    AP_GROUPINFO("SYNAIRSPEED", 27, AP_TECS, _use_synthetic_airspeed, 0),

    // @Param: SMAX_HEIGHT
    // @DisplayName: Maximum height for altitude tracking **NOT IMPLEMENTED YET**
    // @Description: Specifies max height vehicle may rise from water to prevent breaching
    // @Values: 0:Disable,1:Enable
    // @User: Advanced
    AP_GROUPINFO("MAX_HEIGHT", 28, AP_TECS, _mxheight, 0),

    // @Param: FIX_HEIGHT
    // @DisplayName: Constant height setpoint for altitude tracking
    // @Description: Tracks desired height specified directly by rangefinder sensor location
    // @Values: Input units must be in cm; Negative numbers within flyable range of vehicle (-25 to -55 cm appropriate)
    // @User: Advanced
    AP_GROUPINFO("FIX_HEIGHT", 29, AP_TECS, _fxheight, 0),
    
    // @Param: PCH_KP_0
    // @DisplayName: Unscaled proportional gain for pitch
    // @Description: Value representing KP at 1 m/s
    // @Values: ...
    // @User: Advanced
    AP_GROUPINFO("PCH_KP_0", 30, AP_TECS, _pkp_0, 0),
    
    // @Param: PCH_KI_0
    // @DisplayName: Unscaled integral Gain for pitch PID
    // @Description: ...
    // @Values: ...
    // @User: Advanced
    AP_GROUPINFO("PCH_KI_0", 31, AP_TECS, _pki_0, 0),

    // @Param: PCH_KD_0
    // @DisplayName: Unscaled derivative Gain for pitch PID
    // @Description: ...
    // @Values: ...
    // @User: Advanced
    AP_GROUPINFO("PCH_KD_0", 32, AP_TECS, _pkd_0, 0),

    // @Param: IMAX
    // @DisplayName: Integral term limit
    // @Description: Prevent integral windup by setting an upper bound for the term
    // @Values: ...
    // @User: Advanced
    AP_GROUPINFO("IMAX", 33, AP_TECS, _timax, 100),

    // @Param: D_HZ
    // @DisplayName: Cuttoff frequency for derivative term filter used in PID controllers
    // @Description: ...
    // @Values: Value in HZ
    // @User: Advanced
    AP_GROUPINFO("D_HZ", 34, AP_TECS, _thz, 20),

    // @Param: FRCE
    // @DisplayName: Force TECS to run in many situations which it normally would not
    // @Description: ...
    // @Values: 0:Disable,1:Force
    // @User: Advanced
    AP_GROUPINFO("FRCE", 35, AP_TECS, _force, 0),

    // @Param: PCH_PID
    // @DisplayName: Enable height setpoint PID controller for calculating vehicle pitch
    // @Description: ...
    // @Values: ...
    // @User: Advanced
    AP_GROUPINFO("PCH_PID", 36, AP_TECS, _pitch_pid_enable, 0),
        
    // @Param: THT_FRCE
    // @DisplayName: Force the use of manual throttle
    // @Description: Forces uses of 'update_throttle_without_airspeed' method, which is configured for throttle pass through; allows use of airspeed for pitch PID while manually controlling throttle
    // @Values: 0:Disable,1:Enable
    // @User: Advanced
    AP_GROUPINFO("THT_FRCE", 37, AP_TECS, _manual_throt_enable, 0),
           
    // @Param: PCH_FFGN
    // @DisplayName: Feedforward trim pitch values for level flight
    // @Description: Gain for feedforward control
    // @Values: Depends on vehicle configuration; setting to 0 will disable feedforward
    // @User: Advanced
    AP_GROUPINFO("PCH_FFGN", 39, AP_TECS, _pitch_ff_gain, 0.0),   
        
    // @Param: THT_PID
    // @DisplayName: Throttle PID controller
    // @Description: Enable PID controller for calculating vehicle throttle based on airspeed
    // @Values: ...
    // @User: Advanced
    AP_GROUPINFO("THT_PID", 40, AP_TECS, _throttle_pid_enable, 0),
    
    // @Param: THT_KP_0
    // @DisplayName: Unscaled Gain
    // @Description: This is the proportional throttle gain representing flight at 1 m/s
    // @Values: ...
    // @User: Advanced
    AP_GROUPINFO("THT_KP_0", 41, AP_TECS, _tkp_0, 0),
    
    // @Param: THT_KI_0
    // @DisplayName: Integral Gain for throttle PID
    // @Description: ...
    // @Values: ...
    // @User: Advanced
    AP_GROUPINFO("THT_KI_0", 42, AP_TECS, _tki_0, 0),

    // @Param: THT_KD
    // @DisplayName: Derivative Gain for throttle PID
    // @Description: ...
    // @Values: ...
    // @User: Advanced
    AP_GROUPINFO("THT_KD_0", 43, AP_TECS, _tkd_0, 0),
    
    // @Param: ALT_TEST
    // @DisplayName: Test to check possible method of maintaining tracking of TECS_FIX_HEIGHT during waypoint navigation
    // @Description: ...
    // @Values: 0: regular function, 1: Enable test, 2: enable second test
    // @User: Advanced
    AP_GROUPINFO("ALT_TEST", 44, AP_TECS, _alt_test, 0.0f),
    
    // @Param: TEST
    // @DisplayName: Testing changes in stock TECS algorithm (feedforward & potential energy disabled)
    // @Description: ...
    // @Values: 0: regular function, 1: Enable test
    // @User: Advanced
    AP_GROUPINFO("TEST", 45, AP_TECS, _test, 0),

    // @Param: PCHRLL_FF
    // @DisplayName: Gain to enable use of loading factor to increase angle of attack during turns
    // @Description: ...
    // @Values: ...
    // @User: Advanced
    AP_GROUPINFO("PCHRLL_FF", 38, AP_TECS, _pchrll_ff_gain, 0.0f),
    
    // @Param: BNKDSB_PID
    // @DisplayName: Disable aspects of PID controller at a set bank angle
    // @Description: Set which terms to disable at angle specified by 'BNKDSB_ANG'
    // @Values: 0: PID runs, 1: P term disabled, 2: PD disabled, 3: PID disabled
    // @User: Advanced
    AP_GROUPINFO("BNKDSB_PID", 12, AP_TECS, _bank_pid_disable, 0),
    
    // @Param: BNKDSB_ANG
    // @DisplayName: Disable aspects of PID controller at a set bank angle
    // @Description: Set bank angle at which terms specified by 'BNKDSB_PID' shut off
    // @Values: 0-90 deg; setting to 0 or negative value will disable
    // @User: Advanced
    AP_GROUPINFO("BNKDSB_ANG", 13, AP_TECS, _bank_disable_angle, 0.0f),
    
    // @Param: TKOFF_SPD
    // @DisplayName: Speed at which takeoff controller pitches up
    // @Description: ...
    // @Values: UNITS ARE m/s ~ 2-3 m/s for hydrofoil
    // @User: Advanced
    AP_GROUPINFO("TKOFF_SPD", 14, AP_TECS, _takeoff_speed, 3.0f),
    
    // @Param: TKOFF_PTCH
    // @DisplayName: Pitch angle for takeoff rise
    // @Description: Constant angle commanded until desired height is reached
    // @Values: UNITS ARE DEG ~ 8deg for hydrofoil
    // @User: Advanced
    AP_GROUPINFO("TKOFF_PTCH", 17, AP_TECS, _takeoff_pitch, 8.0f),
    
    // @Param: TKOFF_WDW
    // @DisplayName: Percentage of 'FIX_HEIGHT' at which takeoff is complete
    // @Description: ...
    // @Values: Enter a fraction above 1.0; eg. 1.15 completes takeoff at 15% lower than 'FIX_HEIGHT'
    // @User: Advanced
    AP_GROUPINFO("TKOFF_WDW", 18, AP_TECS, _takeoff_window, 1.15f),
    
    // @Param: TKOFF_THT
    // @DisplayName: Constant throttle demand while taking off
    // @Description: ...
    // @Values: 0-1
    // @User: Advanced
    AP_GROUPINFO("TKOFF_THT", 19, AP_TECS, _takeoff_throttle, 0.75f),
    
    
    //FREE SLOTS: 20 21 22 23 24 25 26    
    AP_GROUPEND
};

/* Standing on the back of code written by:
 *  Written by Paul Riseborough 2013
 */

void AP_TECS::update_50hz(void)
{
    // Implement third order complementary filter for height and height rate
    // estimated height rate = _climb_rate
    // estimated height above field elevation  = _height
    // Reference Paper :
    // Optimizing the Gains of the Baro-Inertial Vertical Channel
    // Widnall W.S, Sinha P.K,
    // AIAA Journal of Guidance and Control, 78-1307R

    /*
      if we have a vertical position estimate from the EKF then use
      it, otherwise use barometric altitude
     */
    _ahrs.get_relative_position_D_home(logging.height_log);
    logging.height_log *= -1.0f;

    // Calculate time in seconds since last update
    uint64_t now = AP_HAL::micros64();
    float DT = (now - _update_50hz_last_usec) * 1.0e-6f;
    if (DT > 1.0f) {
        logging.hd_log = 0.0f;
        _height_filter.dd_height = 0.0f;
        DT            = 0.02f; // when first starting TECS, use a
        // small time constant
    }
    _update_50hz_last_usec = now;

    // Use inertial nav verical velocity and height if available
    Vector3f velned;
    if (_ahrs.get_velocity_NED(velned)) {
        // if possible use the EKF vertical velocity
        logging.hd_log = -velned.z;
    } else {
        /*
          use a complimentary filter to calculate climb_rate. This is
          designed to minimise lag
         */
        float baro_alt = _height;
        // Get height acceleration
        float hgt_ddot_mea = -(_ahrs.get_accel_ef().z + GRAVITY_MSS);
        logging.accel_log = _ahrs.get_accel_ef().z;
        // Perform filter calculation using backwards Euler integration
        // Coefficients selected to place all three filter poles at omega
        float omega2 = _hgtCompFiltOmega*_hgtCompFiltOmega;
        float hgt_err = baro_alt - _height_filter.height;
        float integ1_input = hgt_err * omega2 * _hgtCompFiltOmega;

        _height_filter.dd_height += integ1_input * DT;

        float integ2_input = _height_filter.dd_height + hgt_ddot_mea + hgt_err * omega2 * 3.0f;

        logging.hd_log += integ2_input * DT;

        float integ3_input = logging.hd_log + hgt_err * _hgtCompFiltOmega * 3.0f;
        // If more than 1 second has elapsed since last update then reset the integrator state
        // to the measured height
        if (DT > 1.0f) {
            _height_filter.height = logging.height_log;
        } else {
            _height_filter.height += integ3_input*DT;
        }
    }

    // Update and average speed rate of change
    // Get DCM
//    const Matrix3f &rotMat = _ahrs.get_rotation_body_to_ned();
//    // Calculate speed rate of change
//    float temp = rotMat.c.x * GRAVITY_MSS + _ahrs.get_ins().get_accel().x;
//    // take 5 point moving average
//    _vel_dot = _vdot_filter.apply(temp);

}

void AP_TECS::update_50hz(float vz, float pz)
{
    // implement Kalman filter to estimate height above water and climb rate

    /*
      if we have a vertical position estimate from the EKF then use
      it, otherwise use barometric altitude
     */
    //_ahrs.get_relative_position_D_home(_height);
    _height = pz;

    // Calculate time in seconds since last update
    uint64_t now = AP_HAL::micros64();
    float DT = (now - _update_50hz_last_usec) * 1.0e-6f;
    if (DT > 1.0f) {
        _climb_rate = 0.0f;
        _height_filter.dd_height = 0.0f;
        DT            = 0.02f; // when first starting TECS, use a
        // small time constant
    }
    _update_50hz_last_usec = now;

    _climb_rate = vz;

    // Update and average speed rate of change
    // Get DCM
    const Matrix3f &rotMat = _ahrs.get_rotation_body_to_ned();
    // Calculate speed rate of change
    float temp = rotMat.c.x * GRAVITY_MSS + _ahrs.get_ins().get_accel().x;
    // take 5 point moving average
    _vel_dot = _vdot_filter.apply(temp);

}

void AP_TECS::_update_speed(float load_factor)
{
    // Calculate time in seconds since last update
    uint64_t now = AP_HAL::micros64();
    float DT = (now - _update_speed_last_usec) * 1.0e-6f;
    _update_speed_last_usec = now;

    // Convert equivalent airspeeds to true airspeeds

    float EAS2TAS = _ahrs.get_EAS2TAS();
    _TAS_dem = _EAS_dem * EAS2TAS;
    _TASmax   = aparm.airspeed_max * EAS2TAS;
    _TASmin   = aparm.airspeed_min * EAS2TAS;

    if (aparm.stall_prevention) {
        // when stall prevention is active we raise the mimimum
        // airspeed based on aerodynamic load factor
        _TASmin *= load_factor;
    }

    if (_TASmax < _TASmin) {
        _TASmax = _TASmin;
    }
    if (_TASmin > _TAS_dem) {
        _TASmin = _TAS_dem;
    }

    // Reset states of time since last update is too large
    if (DT > 1.0f) {
        _TAS_state = (_EAS * EAS2TAS);
        _integDTAS_state = 0.0f;
        DT            = 0.1f; // when first starting TECS, use a
        // small time constant
    }

    // Get airspeed or default to halfway between min and max if
    // airspeed is not being used and set speed rate to zero
    bool use_airspeed = _use_synthetic_airspeed_once || _use_synthetic_airspeed.get() || _ahrs.airspeed_sensor_enabled();
    if (!use_airspeed || !_ahrs.airspeed_estimate(&_EAS)) {
        // If no airspeed available use average of min and max
        _EAS = 0.5f * (aparm.airspeed_min.get() + (float)aparm.airspeed_max.get());
    }

    // Implement a second order complementary filter to obtain a
    // smoothed airspeed estimate
    // airspeed estimate is held in _TAS_state
    float aspdErr = (_EAS * EAS2TAS) - _TAS_state;
    float integDTAS_input = aspdErr * _spdCompFiltOmega * _spdCompFiltOmega;
    // Prevent state from winding up
    if (_TAS_state < 0.2f) {
        integDTAS_input = MAX(integDTAS_input , 0.0f);
    }
    _integDTAS_state = _integDTAS_state + integDTAS_input * DT;
    float TAS_input = _integDTAS_state + _vel_dot + aspdErr * _spdCompFiltOmega * 1.4142f;
    _TAS_state = _TAS_state + TAS_input * DT;
    // limit the airspeed to a minimum of 3 m/s
    _TAS_state = MAX(_TAS_state, 0.1f);

}

void AP_TECS::_update_speed_demand(void)
{
    // Set the airspeed demand to the minimum value if an underspeed condition exists
    // or a bad descent condition exists
    // This will minimise the rate of descent resulting from an engine failure,
    // enable the maximum climb rate to be achieved and prevent continued full power descent
    // into the ground due to an unachievable airspeed value
//    if ((_flags.badDescent) || (_flags.underspeed))
//    {
//        _TAS_dem     = _TASmin;
//    }

    // Constrain speed demand, taking into account the load factor
    _TAS_dem = constrain_float(_TAS_dem, _TASmin, _TASmax);

    // calculate velocity rate limits based on physical performance limits
    // provision to use a different rate limit if bad descent or underspeed condition exists
    // Use 50% of maximum energy rate to allow margin for total energy controller
    float velRateMax = 0.5f * _STEdot_max / _TAS_state;
    float velRateMin = 0.5f * _STEdot_min / _TAS_state;

    // Apply rate limit
    if ((_TAS_dem - _TAS_dem_adj) > (velRateMax * 0.1f))
    {
        _TAS_dem_adj = _TAS_dem_adj + velRateMax * 0.1f;
        _TAS_rate_dem = velRateMax;
    }
    else if ((_TAS_dem - _TAS_dem_adj) < (velRateMin * 0.1f))
    {
        _TAS_dem_adj = _TAS_dem_adj + velRateMin * 0.1f;
        _TAS_rate_dem = velRateMin;
    }
    else
    {
        _TAS_dem_adj = _TAS_dem;
        _TAS_rate_dem = (_TAS_dem - _TAS_dem_last) / 0.1f;
    }
    // Constrain speed demand again to protect against bad values on initialisation.
    _TAS_dem_adj = constrain_float(_TAS_dem_adj, _TASmin, _TASmax);
    _TAS_dem_last = _TAS_dem;
}

void AP_TECS::_update_energies(void)
{
    // Calculate specific energy demands
    // offset potential energy to maintain positive estimate
    _SPE_dem = (_fxheight * 0.01f * GRAVITY_MSS) + _mxheight;
    _SKE_dem = 0.5f * _TAS_dem_adj * _TAS_dem_adj;

    // Calculate specific energy rate demands
    _SPEdot_dem = 0.0f;
    _SKEdot_dem = _TAS_state * _TAS_rate_dem;

    // Calculate specific energy
    _SPE_est = (_height * GRAVITY_MSS) + _mxheight;
    _SKE_est = 0.5f * _TAS_state * _TAS_state;

    // Calculate specific energy rate
    _SPEdot = _climb_rate * GRAVITY_MSS;
    _SKEdot = _TAS_state * _vel_dot;

}

/*
  current time constant. It is lower in landing to try to give a precise approach
 */
float AP_TECS::timeConstant(void) const
{
//    if (_flags.is_doing_auto_land) {
//        if (_landTimeConst < 0.1f) {
//            return 0.1f;
//        }
//        return _landTimeConst;
//    }
    if (_timeConst < 0.1f) {
        return 0.1f;
    }
    return _timeConst;
}

// ## TO-DO : Change Airplane State Behaviors to how hydrofoil will perform

/*
  calculate throttle demand - airspeed enabled case
 */
void AP_TECS::_update_throttle_with_airspeed(void)
{
    // Calculate limits to be applied to potential energy error to prevent over or underspeed occurring due to large height errors
    float SPE_err_max = 0.5f * _TASmax * _TASmax - _SKE_dem;
    float SPE_err_min = 0.5f * _TASmin * _TASmin - _SKE_dem;

    if(_test == 1) {
        SPE_err_max = SPE_err_min = 0; 
    }

    if (_flight_stage == AP_Vehicle::FixedWing::FLIGHT_VTOL) {
        /*
          when we are in a VTOL state then we ignore potential energy
          errors as we have vertical motors that interfere with the
          total energy calculation.
         */
        SPE_err_max = SPE_err_min = 0;
    }
    
    // Calculate total energy error
    _STE_error = constrain_float((_SPE_dem - _SPE_est), SPE_err_min, SPE_err_max) + _SKE_dem - _SKE_est;
    float STEdot_dem = constrain_float((_SPEdot_dem + _SKEdot_dem), _STEdot_min, _STEdot_max);
    float STEdot_error = STEdot_dem - _SPEdot - _SKEdot;

    // Apply 0.5 second first order filter to STEdot_error
    // This is required to remove accelerometer noise from the  measurement
    STEdot_error = 0.2f*STEdot_error + 0.8f*_STEdotErrLast;
    _STEdotErrLast = STEdot_error;

    // Calculate throttle demand
    // If underspeed condition is set, then demand full throttle
    if (_flags.underspeed)
    {
        _throttle_dem = 1.0f;
    }
    else
    {
        // Calculate gain scaler from specific energy error to throttle
        float K_STE2Thr = 1 / (timeConstant() * (_STEdot_max - _STEdot_min));

        // Calculate feed-forward throttle
        float ff_throttle = 0;
        float nomThr = aparm.throttle_cruise * 0.01f;
        const Matrix3f &rotMat = _ahrs.get_rotation_body_to_ned();
        // Use the demanded rate of change of total energy as the feed-forward demand, but add
        // additional component which scales with (1/cos(bank angle) - 1) to compensate for induced
        // drag increase during turns.
        float cosPhi = sqrtf((rotMat.a.y*rotMat.a.y) + (rotMat.b.y*rotMat.b.y));
        STEdot_dem = STEdot_dem + _rollComp * (1.0f/constrain_float(cosPhi * cosPhi , 0.1f, 1.0f) - 1.0f);
        if(_test == 1) {
            ff_throttle = 0.1074f * _TAS_state + 0.114f; 
        }
        else {
            ff_throttle = nomThr + STEdot_dem / (_STEdot_max - _STEdot_min) * (_THRmaxf - _THRminf);
        }
        // Calculate PD + FF throttle
        float throttle_damp = _thrDamp;
//        if (_flags.is_doing_auto_land && !is_zero(_land_throttle_damp)) {
//            throttle_damp = _land_throttle_damp;
//        }
        _throttle_dem = (_STE_error + STEdot_error * throttle_damp) * K_STE2Thr + ff_throttle;

        // Constrain throttle demand
        _throttle_dem = constrain_float(_throttle_dem, _THRminf, _THRmaxf);

        float THRminf_clipped_to_zero = constrain_float(_THRminf, 0, _THRmaxf);

        // Rate limit PD + FF throttle
        // Calculate the throttle increment from the specified slew time
        if (aparm.throttle_slewrate != 0) {
            float thrRateIncr = _DT * (_THRmaxf - THRminf_clipped_to_zero) * aparm.throttle_slewrate * 0.01f;

            _throttle_dem = constrain_float(_throttle_dem,
                                            _last_throttle_dem - thrRateIncr,
                                            _last_throttle_dem + thrRateIncr);
            _last_throttle_dem = _throttle_dem;
        }

        // Calculate integrator state upper and lower limits
        // Set to a value that will allow 0.1 (10%) throttle saturation to allow for noise on the demand
        // Additionally constrain the integrator state amplitude so that the integrator comes off limits faster.
        float maxAmp = 0.5f*(_THRmaxf - THRminf_clipped_to_zero);
        float integ_max = constrain_float((_THRmaxf - _throttle_dem + 0.1f),-maxAmp,maxAmp);
        float integ_min = constrain_float((_THRminf - _throttle_dem - 0.1f),-maxAmp,maxAmp);

        // Calculate integrator state, constraining state
        // Set integrator to a max throttle value during climbout
        _integTHR_state = _integTHR_state + (_STE_error * _get_i_gain()) * _DT * K_STE2Thr;
        if (_flight_stage == AP_Vehicle::FixedWing::FLIGHT_TAKEOFF || _flight_stage == AP_Vehicle::FixedWing::FLIGHT_ABORT_LAND)
        {
            if (!_flags.reached_speed_takeoff) {
                // ensure we run at full throttle until we reach the target airspeed
                _throttle_dem = MAX(_throttle_dem, _THRmaxf - _integTHR_state);
            }
            _integTHR_state = integ_max;
        }
        else
        {
            _integTHR_state = constrain_float(_integTHR_state, integ_min, integ_max);
        }

        // Sum the components.
        _throttle_dem = _throttle_dem + _integTHR_state;
    }

    // Constrain throttle demand
    _throttle_dem = constrain_float(_throttle_dem, _THRminf, _THRmaxf);
}

float AP_TECS::_get_i_gain(void)
{
//    float i_gain = _integGain;
//    if (_flight_stage == AP_Vehicle::FixedWing::FLIGHT_TAKEOFF || _flight_stage == AP_Vehicle::FixedWing::FLIGHT_ABORT_LAND) {
//        if (!is_zero(_integGain_takeoff)) {
//            i_gain = _integGain_takeoff;
//        }
//    } //else if (_flags.is_doing_auto_land) {
//        if (!is_zero(_integGain_land)) {
//            i_gain = _integGain_land;
//        }
//    }
    return _integGain;
}

/*
  calculate throttle, non-airspeed case
 */
void AP_TECS::_update_throttle_without_airspeed(int16_t throttle_nudge)
{
    // Calculate throttle demand by interpolating between pitch and throttle limits
    float nomThr;
    nomThr = throttle_nudge * 0.01f;

//    if (_flags.is_doing_auto_land && _landThrottle >= 0) {
//        nomThr = (_landThrottle + throttle_nudge) * 0.01f;

/*	Bypassed for constant throttle
    if (_pitch_dem > 0.0f && _PITCHmaxf > 0.0f)
    {
        _throttle_dem = nomThr + (_THRmaxf - nomThr) * _pitch_dem / _PITCHmaxf;
    }
    else if (_pitch_dem < 0.0f && _PITCHminf < 0.0f)
    {
        _throttle_dem = nomThr + (_THRminf - nomThr) * _pitch_dem / _PITCHminf;
    }
    else
    {
        _throttle_dem = nomThr;
    }

    // Calculate additional throttle for turn drag compensation including throttle nudging
    const Matrix3f &rotMat = _ahrs.get_rotation_body_to_ned();
    // Use the demanded rate of change of total energy as the feed-forward demand, but add
    // additional component which scales with (1/cos(bank angle) - 1) to compensate for induced
    // drag increase during turns.
    float cosPhi = sqrtf((rotMat.a.y*rotMat.a.y) + (rotMat.b.y*rotMat.b.y));
    float STEdot_dem = _rollComp * (1.0f/constrain_float(cosPhi * cosPhi , 0.1f, 1.0f) - 1.0f);
    _throttle_dem = _throttle_dem + STEdot_dem / (_STEdot_max - _STEdot_min) * (_THRmaxf - _THRminf);
*/
    _throttle_dem = constrain_float(nomThr, _THRminf, _THRmaxf);
}

void AP_TECS::_update_pitch(void)
{
    // Calculate Speed/Height Control Weighting
    // This is used to determine how the pitch control prioritises speed and height control
    // A weighting of 1 provides equal priority (this is the normal mode of operation)
    // A SKE_weighting of 0 provides 100% priority to height control. This is used when no airspeed measurement is available
    // A SKE_weighting of 2 provides 100% priority to speed control. This is used when an underspeed condition is detected. In this instance, if airspeed
    // rises above the demanded value, the pitch angle will be increased by the TECS controller.
    float SKE_weighting = constrain_float(_spdWeight, 0.0f, 2.0f);
    if (!_ahrs.airspeed_sensor_enabled()) {
        SKE_weighting = 0.0f;
    } else if (_flight_stage == AP_Vehicle::FixedWing::FLIGHT_VTOL) {
        // if we are in VTOL mode then control pitch without regard to
        // speed. Speed is also taken care of independently of
        // height. This is needed as the usual relationship of speed
        // and height is broken by the VTOL motors
        SKE_weighting = 0.0f;        
    } else if ( _flags.underspeed || _flight_stage == AP_Vehicle::FixedWing::FLIGHT_TAKEOFF || _flight_stage == AP_Vehicle::FixedWing::FLIGHT_ABORT_LAND) {
        SKE_weighting = 2.0f;
    } //else if (_flags.is_doing_auto_land) {
       //        if (_spdWeightLand < 0) {
       //            // use sliding scale from normal weight down to zero at landing
       //            float scaled_weight = _spdWeight * (1.0f - constrain_float(_path_proportion,0,1));
       //            SKE_weighting = constrain_float(scaled_weight, 0.0f, 2.0f);
       //        } else {
       //            SKE_weighting = constrain_float(_spdWeightLand, 0.0f, 2.0f);
       //        }
       //    }

    logging.SKE_weighting = SKE_weighting;
    
    float SPE_weighting = 2.0f - SKE_weighting;

    // Calculate Specific Energy Balance demand, and error
    float SEB_dem      = _SPE_dem * SPE_weighting - _SKE_dem * SKE_weighting;
    float SEBdot_dem   = _SPEdot_dem * SPE_weighting - _SKEdot_dem * SKE_weighting;
    float SEB_error    = SEB_dem - (_SPE_est * SPE_weighting - _SKE_est * SKE_weighting);
    float SEBdot_error = SEBdot_dem - (_SPEdot * SPE_weighting - _SKEdot * SKE_weighting);

    logging.SKE_error = _SKE_dem - _SKE_est;
    logging.SPE_error = _SPE_dem - _SPE_est;
    
    // Calculate integrator state, constraining input if pitch limits are exceeded
    float integSEB_input = SEB_error * _get_i_gain();
    if (_pitch_dem > _PITCHmaxf)
    {
        integSEB_input = MIN(integSEB_input, _PITCHmaxf - _pitch_dem);
    }
    else if (_pitch_dem < _PITCHminf)
    {
        integSEB_input = MAX(integSEB_input, _PITCHminf - _pitch_dem);
    }
    float integSEB_delta = integSEB_input * _DT;

#if 0
    if (_landing.is_flaring() && fabsf(_climb_rate) > 0.2f) {
        ::printf("_hgt_rate_dem=%.1f _hgt_dem_adj=%.1f climb=%.1f _flare_counter=%u _pitch_dem=%.1f SEB_dem=%.2f SEBdot_dem=%.2f SEB_error=%.2f SEBdot_error=%.2f\n",
                 _hgt_rate_dem, _hgt_dem_adj, _climb_rate, _flare_counter, degrees(_pitch_dem),
                 SEB_dem, SEBdot_dem, SEB_error, SEBdot_error);
    }
#endif


    // Apply max and min values for integrator state that will allow for no more than
    // 5deg of saturation. This allows for some pitch variation due to gusts before the
    // integrator is clipped. Otherwise the effectiveness of the integrator will be reduced in turbulence
    // During climbout/takeoff, bias the demanded pitch angle so that zero speed error produces a pitch angle
    // demand equal to the minimum value (which is )set by the mission plan during this mode). Otherwise the
    // integrator has to catch up before the nose can be raised to reduce speed during climbout.
    // During flare a different damping gain is used
    float gainInv = (_TAS_state * timeConstant() * GRAVITY_MSS);
    float temp = SEB_error + SEBdot_dem * timeConstant();

    float pitch_damp = _ptchDamp;
//    if (_landing.is_flaring()) {
//        pitch_damp = _landDamp;
//    } else if (!is_zero(_land_pitch_damp) && _flags.is_doing_auto_land) {
//        pitch_damp = _land_pitch_damp;
//    }
    temp += SEBdot_error * pitch_damp;

    if (_flight_stage == AP_Vehicle::FixedWing::FLIGHT_TAKEOFF || _flight_stage == AP_Vehicle::FixedWing::FLIGHT_ABORT_LAND) {
        temp += _PITCHminf * gainInv;
    }
    float integSEB_min = (gainInv * (_PITCHminf - 0.0783f)) - temp;
    float integSEB_max = (gainInv * (_PITCHmaxf + 0.0783f)) - temp;
    float integSEB_range = integSEB_max - integSEB_min;

    logging.SEB_delta = integSEB_delta;
    
    // don't allow the integrator to rise by more than 20% of its full
    // range in one step. This prevents single value glitches from
    // causing massive integrator changes. See Issue#4066
    integSEB_delta = constrain_float(integSEB_delta, -integSEB_range*0.1f, integSEB_range*0.1f);

    // integrate
    _integSEB_state = constrain_float(_integSEB_state + integSEB_delta, integSEB_min, integSEB_max);

    // Calculate pitch demand from specific energy balance signals
    _pitch_dem_unc = (temp + _integSEB_state) / gainInv;

    // Constrain pitch demand
    _pitch_dem = constrain_float(_pitch_dem_unc, _PITCHminf, _PITCHmaxf);

    // Rate limit the pitch demand to comply with specified vertical
    // acceleration limit
    float ptchRateIncr = _DT * _vertAccLim / _TAS_state;

    if ((_pitch_dem - _last_pitch_dem) > ptchRateIncr)
    {
        _pitch_dem = _last_pitch_dem + ptchRateIncr;
    }
    else if ((_pitch_dem - _last_pitch_dem) < -ptchRateIncr)
    {
        _pitch_dem = _last_pitch_dem - ptchRateIncr;
    }

    // re-constrain pitch demand
    _pitch_dem = constrain_float(_pitch_dem, _PITCHminf, _PITCHmaxf);

    _last_pitch_dem = _pitch_dem;
}

void AP_TECS::_calc_takeoff(void) {
    if(_TAS_state < _takeoff_speed) {
        _pitch_dem = 0.0f;
    }
    else _pitch_dem = constrain_float(_takeoff_pitch, _PITCHminf, _PITCHmaxf);; 
    
    _throttle_dem = constrain_float(_takeoff_throttle, _THRminf, _THRmaxf);
}

// configured to takeoff only once per activation of FBWB
// Required to re-takeoff: User exits FBWB for >1sec, Speed and height are minimal (preferably stopped)
void AP_TECS::_calc_takeoff_flag(void) {

    // final speed condition needs to be dialed in. Must define a flying speed, but for now this may mess with tuning the takeoff
    if(!_flags._takeoff_complete && (_height > _fxheight* constrain_float(_takeoff_window, 1.0f, 2.0f) || _TAS_state > _takeoff_speed * 2.0f)) {
        _flags._takeoff_complete = true;
    }
    
}

//NOTE: Pitch has units of radians
void AP_TECS::_update_pitch_pid(void)
{
    uint32_t tnow = AP_HAL::millis();
    uint32_t dt = tnow - _last_t_p;
    float delta_time;
    float output = 0.0;
     
    // feedfoward trim pitch from theoretically derived values
    output += _pitch_ff_gain/(_TAS_state*_TAS_state);
    
    if(_pchrll_ff_gain > 0.0f){
        const Matrix3f &rotMat = _ahrs.get_rotation_body_to_ned();
        _cPhi = sqrtf((rotMat.a.y*rotMat.a.y) + (rotMat.b.y*rotMat.b.y));
        output *= _pchrll_ff_gain * ((1.0f/_cPhi) + 1.0f);
    }
    
    if (_last_t_p == 0 || dt > 1000) {
        dt = 0;

		// if this PID hasn't been used for a full second then zero
		// the intergator term. This prevents I buildup from a
		// previous fight mode from causing a massive return before
		// the integrator gets a chance to correct itself
		_integrator_pid_p = 0.0;
    }
    _last_t_p = tnow;

    delta_time = (float)dt / 1000.0f;
    float error = _height-(_fxheight * 0.01f);
    
    // scale PID gains based on velocity
    _pkp = _pkp_0/(_TAS_state*_TAS_state);
    _pkd = _pkd_0/(_TAS_state*_TAS_state);
    _pki = _pki_0/(_TAS_state*_TAS_state);

    // add 'break' statements to prevent unnecessary comparisons?
    if(labs(_ahrs.roll_sensor) > _bank_disable_angle*100 && _bank_disable_angle > 0.0f) {
        if(_bank_pid_disable >= 1) {
            _pkp = 0.0f;
        }
        if(_bank_pid_disable >= 2) {
            _pkd = 0.0f;
        }
        if(_bank_pid_disable >= 3) {
            _pki = 0.0f;
        }
    }
    // Compute proportional component
    output += (_pkp*error);

    // Compute derivative component if time has elapsed
    if ((fabsf(_pkd) > 0) && (dt > 0)) {
        float derivative;

		if (isnan(_last_derivative_p)) {
			// we've just done a reset, suppress the first derivative
			// term as we don't want a sudden change in input to cause
			// a large D output change			
			derivative = 0;
			_last_derivative_p = 0;
		} else {
			derivative = (error - _last_error_p) / delta_time;
		}

        // discrete low pass filter, cuts out the
        // high frequency noise that can drive the controller crazy
        float RC = 1/(2*M_PI*_thz);
        derivative = _last_derivative_p +
                     ((delta_time / (RC + delta_time)) *
                      (derivative - _last_derivative_p));

        // update state
        _last_error_p         = error;
        _last_derivative_p    = derivative;

        // add in derivative component
        output += (_pkd * derivative);
    }

    // Compute integral component if time has elapsed
    if ((fabsf(_pki) > 0) && (dt > 0)) {
        _integrator_pid_p             += error * delta_time;
        if (_integrator_pid_p < -_timax) {
            _integrator_pid_p = -_timax;
        } else if (_integrator_pid_p > _timax) {
            _integrator_pid_p = _timax;
        }
        output                          += _pki * _integrator_pid_p;
    }
    
    _pitch_dem = constrain_float(output, _PITCHminf, _PITCHmaxf);

    _last_pitch_dem = _pitch_dem;
 
}

void AP_TECS::_update_throttle_pid(void)
{
    
    uint32_t tnow = AP_HAL::millis();
    uint32_t dt = tnow - _last_t_t;
    float delta_time;
    float output = 0.0;
    
    output += 0.1074f * _TAS_state + 0.114f;
    
    if (_last_t_t == 0 || dt > 1000) {
        dt = 0;

		// if this PID hasn't been used for a full second then zero
		// the intergator term. This prevents I buildup from a
		// previous fight mode from causing a massive return before
		// the integrator gets a chance to correct itself
		_integrator_pid_t=0.0;
    }
    _last_t_t = tnow;

    delta_time = (float)dt / 1000.0f;
    float error = _TAS_state-_TAS_dem;
    
    _tkp = _tkp_0; //* _TAS_state;
    _tki = _tki_0;
    _tkd = _tkd_0;
    
    // Compute proportional component
    output += (_tkp*error);

    // Compute derivative component if time has elapsed
    if ((fabsf(_tkd) > 0) && (dt > 0)) {
        float derivative;

		if (isnan(_last_derivative_t)) {
			// we've just done a reset, suppress the first derivative
			// term as we don't want a sudden change in input to cause
			// a large D output change			
			derivative = 0;
			_last_derivative_t = 0;
		} else {
			derivative = (error - _last_error_t) / delta_time;
		}

        // discrete low pass filter, cuts out the
        // high frequency noise that can drive the controller crazy
        float RC = 1/(2*M_PI*_thz);
        derivative = _last_derivative_t +
                     ((delta_time / (RC + delta_time)) *
                      (derivative - _last_derivative_t));

        // update state
        _last_error_t         = error;
        _last_derivative_t    = derivative;

        // add in derivative component
        output += (_tkd * derivative);
    }

    // Compute integral component if time has elapsed
    if ((fabsf(_tki) > 0) && (dt > 0)) {
        _integrator_pid_t             += (error * _tki) * delta_time;
        if (_integrator_pid_t < -_timax) {
            _integrator_pid_t = -_timax;
        } else if (_integrator_pid_t > _timax) {
            _integrator_pid_t = _timax;
        }
        output                          += _integrator_pid_t;
    }
    _throttle_dem = constrain_float(output, _THRminf, _THRmaxf);

    _last_throttle_dem = _throttle_dem;
 
}

void AP_TECS::_initialise_states(int32_t ptchMinCO_cd, float hgt_afe)
{
    // Initialise states and variables if DT > 1 second or in climbout
    if (_DT > 1.0f)
    {
	_integrator_pid_p	     = 0.0f;
	_integrator_pid_t	     = 0.0f;
        _integTHR_state      = 0.0f;
        _integSEB_state      = 0.0f;
        _last_throttle_dem = aparm.throttle_cruise * 0.01f;
        _last_pitch_dem    = _ahrs.pitch;
        _hgt_dem_adj_last  = _fxheight * 0.01f;
        _hgt_dem_adj       = _fxheight * 0.01f;
        _hgt_dem_prev      = _fxheight * 0.01f;
        _hgt_dem_in_old    = _fxheight * 0.01f;
        _TAS_dem_last      = _TAS_dem;
        _TAS_dem_adj       = _TAS_dem;
        _flags.underspeed        = false;
        _flags.badDescent        = false;
        _flags.reached_speed_takeoff = false;
        _flags._takeoff_complete = false;
        _DT                = 0.1f; // when first starting TECS, use a
        // small time constant
    }
    else if (_flight_stage == AP_Vehicle::FixedWing::FLIGHT_TAKEOFF || _flight_stage == AP_Vehicle::FixedWing::FLIGHT_ABORT_LAND)
    {
        _PITCHminf          = 0.000174533f * ptchMinCO_cd;
        _hgt_dem_adj_last  = _fxheight * 0.01f;
        _hgt_dem_adj       = _fxheight * 0.01f;
        _hgt_dem_prev      = _fxheight * 0.01f;
        _TAS_dem_last      = _TAS_dem;
        _TAS_dem_adj       = _TAS_dem;
        _flags.underspeed        = false;
        _flags.badDescent  = false;
    }
    
    if (_flight_stage != AP_Vehicle::FixedWing::FLIGHT_TAKEOFF && _flight_stage != AP_Vehicle::FixedWing::FLIGHT_ABORT_LAND) {
        // reset takeoff speed flag when not in takeoff
        _flags.reached_speed_takeoff = false;        
    }
}

void AP_TECS::_update_STE_rate_lim(void)
{
    // Calculate Specific Total Energy Rate Limits
    // This is a trivial calculation at the moment but will get bigger once we start adding altitude effects
    _STEdot_max = _maxClimbRate * GRAVITY_MSS;
    _STEdot_min = - _minSinkRate * GRAVITY_MSS;
}


void AP_TECS::update_pitch_throttle(int32_t hgt_dem_cm,
                                    int32_t EAS_dem_cm,
                                    enum AP_Vehicle::FixedWing::FlightStage flight_stage,
                                    float distance_beyond_land_wp,
                                    int32_t ptchMinCO_cd,
                                    int16_t throttle_nudge,
                                    float hgt_afe,
                                    float load_factor)
{
    // Calculate time in seconds since last update
    uint64_t now = AP_HAL::micros64();
    _DT = (now - _update_pitch_throttle_last_usec) * 1.0e-6f;
    _update_pitch_throttle_last_usec = now;

    _flags.is_doing_auto_land = false; //(flight_stage == AP_Vehicle::FixedWing::FLIGHT_LAND); --> disable landing mode for hydrofoil (just kill the throttle)
    _distance_beyond_land_wp = distance_beyond_land_wp;
    _flight_stage = flight_stage;

    // Convert inputs
    _hgt_dem = _fxheight * 0.01f;
    _EAS_dem = EAS_dem_cm * 0.01f;

    // Update the speed estimate using a 2nd order complementary filter
    _update_speed(load_factor);

    if (aparm.takeoff_throttle_max != 0 &&
            (_flight_stage == AP_Vehicle::FixedWing::FLIGHT_TAKEOFF || _flight_stage == AP_Vehicle::FixedWing::FLIGHT_ABORT_LAND)) {
        _THRmaxf  = aparm.takeoff_throttle_max * 0.01f;
    } else {
        _THRmaxf  = aparm.throttle_max * 0.01f;
    }
    _THRminf  = aparm.throttle_min * 0.01f;

    // work out the maximum and minimum pitch
    // if TECS_PITCH_{MAX,MIN} isn't set then use
    // LIM_PITCH_{MAX,MIN}. Don't allow TECS_PITCH_{MAX,MIN} to be
    // larger than LIM_PITCH_{MAX,MIN}
    if (_pitch_max <= 0) {
        _PITCHmaxf = aparm.pitch_limit_max_cd * 0.01f;
    } else {
        _PITCHmaxf = MIN(_pitch_max, aparm.pitch_limit_max_cd * 0.01f);
    }

    if (_pitch_min >= 0) {
        _PITCHminf = aparm.pitch_limit_min_cd * 0.01f;
    } else {
        _PITCHminf = MAX(_pitch_min, aparm.pitch_limit_min_cd * 0.01f);
    }

    // apply temporary pitch limit and clear
    if (_pitch_max_limit < 90) {
        _PITCHmaxf = constrain_float(_PITCHmaxf, -90, _pitch_max_limit);
        _PITCHminf = constrain_float(_PITCHminf, -_pitch_max_limit, _PITCHmaxf);
        _pitch_max_limit = 90;
    }
        
//    if (_landing.is_flaring()) {
//        // in flare use min pitch from LAND_PITCH_CD
//        _PITCHminf = MAX(_PITCHminf, _landing.get_pitch_cd() * 0.01f);
//
//        // and use max pitch from TECS_LAND_PMAX
//        if (_land_pitch_max != 0) {
//            _PITCHmaxf = MIN(_PITCHmaxf, _land_pitch_max);
//        }
//
//        // and allow zero throttle
//        _THRminf = 0;
//    } else if (_landing.is_on_approach() && (-_climb_rate) > _land_sink) {
//        // constrain the pitch in landing as we get close to the flare
//        // point. Use a simple linear limit from 15 meters after the
//        // landing point
//        float time_to_flare = (- hgt_afe / _climb_rate) - _landing.get_flare_sec();
//        if (time_to_flare < 0) {
//            // we should be flaring already
//            _PITCHminf = MAX(_PITCHminf, _landing.get_pitch_cd() * 0.01f);
//        } else if (time_to_flare < timeConstant()*2) {
//            // smoothly move the min pitch to the flare min pitch over
//            // twice the time constant
//            float p = time_to_flare/(2*timeConstant());
//            float pitch_limit_cd = p*aparm.pitch_limit_min_cd + (1-p)*_landing.get_pitch_cd();
//#if 0
//            ::printf("ttf=%.1f hgt_afe=%.1f _PITCHminf=%.1f pitch_limit=%.1f climb=%.1f\n",
//                     time_to_flare, hgt_afe, _PITCHminf, pitch_limit_cd*0.01f, _climb_rate);
//#endif
//            _PITCHminf = MAX(_PITCHminf, pitch_limit_cd*0.01f);
//        }
//    }

//    if (flight_stage == AP_Vehicle::FixedWing::FLIGHT_TAKEOFF || flight_stage == AP_Vehicle::FixedWing::FLIGHT_ABORT_LAND) {
//        if (!_flags.reached_speed_takeoff && _TAS_state >= _TAS_dem_adj) {
//            // we have reached our target speed in takeoff, allow for
//            // normal throttle control
//            _flags.reached_speed_takeoff = true;
//        }
//    }
    
    // convert to radians
    _PITCHmaxf = radians(_PITCHmaxf);
    _PITCHminf = radians(_PITCHminf);

    // initialise selected states and variables if DT > 1 second or in climbout
    _initialise_states(ptchMinCO_cd, hgt_afe);
    
    // determine whether takeoff is required
    _calc_takeoff_flag();
    
    // Calculate Specific Total Energy Rate Limits
    _update_STE_rate_lim();

    // Calculate the speed demand
    _update_speed_demand();

    // Calculate the height demand
//    _update_height_demand();

    // Detect underspeed condition
    //_detect_underspeed();

    // Calculate specific energy quantities
    _update_energies();

    // Calculate throttle demand - use simple pitch to throttle if no
    // airspeed sensor.
    // Note that caller can demand the use of
    // synthetic airspeed for one loop if needed. This is required
    // during QuadPlane transition when pitch is constrained
     //if(_flags._takeoff_complete) {
        if (_throttle_pid_enable >= 1.0) {
            _update_throttle_pid();
        }
        else if (((_ahrs.airspeed_sensor_enabled() || _use_synthetic_airspeed) && !_manual_throt_enable) || _use_synthetic_airspeed_once) {
            _update_throttle_with_airspeed();
            _use_synthetic_airspeed_once = false;
        } else {
            _update_throttle_without_airspeed(throttle_nudge);
        }
    
        // Detect bad descent due to demanded airspeed being too high
    //    _detect_bad_descent();
    
        // Calculate pitch demand
        if (_pitch_pid_enable >= 1.0) {_update_pitch_pid();}
        else {_update_pitch();}
    //}
//    else {
//        _calc_takeoff();
//    }
    // log to DataFlash
    DataFlash_Class::instance()->Log_Write("TECS", "TimeUS,h,dh,hdem,dhdem,spdem,sp,dsp,ith,iph,th,ph,dspdem,w,f", "QfffffffffffffB",
                                           now,
                                           (double)_height,
                                           (double)_climb_rate,
                                           (double)_hgt_dem_adj,
                                           (double)_hgt_rate_dem,
                                           (double)_TAS_dem_adj,
                                           (double)_TAS_state,
                                           (double)_vel_dot,
                                           (double)_integTHR_state,
                                           (double)_integSEB_state,
                                           (double)_throttle_dem,
                                           (double)_pitch_dem,
                                           (double)_TAS_rate_dem,
                                           (double)logging.SKE_weighting,
                                           _flags_byte);
    DataFlash_Class::instance()->Log_Write("TEC2", "TimeUS,KErr,PErr,EDelta,LF,integ,cPhi, h_kf, hd_kf, accZ", "Qfffffffff",
                                           now,
                                           (double)logging.SKE_error,
                                           (double)logging.SPE_error,
                                           (double)logging.SEB_delta,
                                           (double)load_factor,
                                           (double)_integrator_pid_p,
                                           (double)_cPhi,
                                           (double)logging.height_log,
                                           (double)logging.hd_log,
                                           (double)logging.accel_log);
}





//// OLD JUNK ////
// REMOVED FOR ARDUFOIL

//void AP_TECS::_update_height_demand(void)
//{
//    /* This is being commented out for a fixed height demand
//    // Apply 2 point moving average to demanded height
//    _hgt_dem = 0.5f * (_hgt_dem + _hgt_dem_in_old);
//    _hgt_dem_in_old = _hgt_dem;
//
//    float max_sink_rate = _maxSinkRate;
//    if (_maxSinkRate_approach > 0 && _flags.is_doing_auto_land) {
//        // special sink rate for approach to accommodate steep slopes and reverse thrust.
//        // A special check must be done to see if we're LANDing on approach but also if
//        // we're in that tiny window just starting NAV_LAND but still in NORMAL mode. If
//        // we have a steep slope with a short approach we'll want to allow acquiring the
//        // glide slope right away.
//        max_sink_rate = _maxSinkRate_approach;
//    }
//
//    // Limit height rate of change
//    if ((_hgt_dem - _hgt_dem_prev) > (_maxClimbRate * 0.1f))
//    {
//        _hgt_dem = _hgt_dem_prev + _maxClimbRate * 0.1f;
//    }
//    else if ((_hgt_dem - _hgt_dem_prev) < (-max_sink_rate * 0.1f))
//    {
//        _hgt_dem = _hgt_dem_prev - max_sink_rate * 0.1f;
//    }
//    _hgt_dem_prev = _hgt_dem;
//
//    // Apply first order lag to height demand
//    _hgt_dem_adj = 0.05f * _hgt_dem + 0.95f * _hgt_dem_adj_last;
//
//    // when flaring force height rate demand to the
//    // configured sink rate and adjust the demanded height to
//    // be kinematically consistent with the height rate.
//    if (_landing.is_flaring()) {
//        _integSEB_state = 0;
//        if (_flare_counter == 0) {
//            _hgt_rate_dem = _climb_rate;
//            _land_hgt_dem = _hgt_dem_adj;
//        }
//
//        // adjust the flare sink rate to increase/decrease as your travel further beyond the land wp
//        float land_sink_rate_adj = _land_sink + _land_sink_rate_change*_distance_beyond_land_wp;
//
//        // bring it in over 1s to prevent overshoot
//        if (_flare_counter < 10) {
//            _hgt_rate_dem = _hgt_rate_dem * 0.8f - 0.2f * land_sink_rate_adj;
//            _flare_counter++;
//        } else {
//            _hgt_rate_dem = - land_sink_rate_adj;
//        }
//        _land_hgt_dem += 0.1f * _hgt_rate_dem;
//        _hgt_dem_adj = _land_hgt_dem;
//    } else {
//        _hgt_rate_dem = (_hgt_dem_adj - _hgt_dem_adj_last) / 0.1f;
//        _flare_counter = 0;
//    }
//
//    // for landing approach we will predict ahead by the time constant
//    // plus the lag produced by the first order filter. This avoids a
//    // lagged height demand while constantly descending which causes
//    // us to consistently be above the desired glide slope. This will
//    // be replaced with a better zero-lag filter in the future.
//    float new_hgt_dem = _hgt_dem_adj;
//    if (_flags.is_doing_auto_land) {
//        if (hgt_dem_lag_filter_slew < 1) {
//            hgt_dem_lag_filter_slew += 0.1f; // increment at 10Hz to gradually apply the compensation at first
//        } else {
//            hgt_dem_lag_filter_slew = 1;
//        }
//        new_hgt_dem += hgt_dem_lag_filter_slew*(_hgt_dem_adj - _hgt_dem_adj_last)*10.0f*(timeConstant()+1);
//    } else {
//        hgt_dem_lag_filter_slew = 0;
//    }
//    _hgt_dem_adj_last = _hgt_dem_adj;
//    _hgt_dem_adj = new_hgt_dem;
//	*/
//    _hgt_dem_adj_last = _fxheight * 0.01f;
//    _hgt_dem_adj = _fxheight * 0.01f;
//}

//void AP_TECS::_detect_underspeed(void)
//{
//    // see if we can clear a previous underspeed condition. We clear
//    // it if we are now more than 15% above min speed, and haven't
//    // been below min speed for at least 3 seconds.
//    if (_flags.underspeed &&
//        _TAS_state >= _TASmin * 1.15f &&
//        AP_HAL::millis() - _underspeed_start_ms > 3000U) {
//        _flags.underspeed = false;
//    }
//
//    if (_flight_stage == AP_Vehicle::FixedWing::FLIGHT_VTOL) {
//        _flags.underspeed = false;
//    } else if (((_TAS_state < _TASmin * 0.9f) &&
//            (_throttle_dem >= _THRmaxf * 0.95f) &&
//            !_landing.is_flaring()) ||
//            ((_height < _hgt_dem_adj) && _flags.underspeed))
//    {
//        _flags.underspeed = true;
//        if (_TAS_state < _TASmin * 0.9f) {
//            // reset start time as we are still underspeed
//            _underspeed_start_ms = AP_HAL::millis();
//        }
//    }
//    else
//    {
//        // this clears underspeed if we reach our demanded height and
//        // we are either below 95% throttle or we above 90% of min
//        // airspeed
//        _flags.underspeed = false;
//    }
//}

//void AP_TECS::_detect_bad_descent(void)
//{
//    // Detect a demanded airspeed too high for the aircraft to achieve. This will be
//    // evident by the the following conditions:
//    // 1) Underspeed protection not active
//    // 2) Specific total energy error > 200 (greater than ~20m height error)
//    // 3) Specific total energy reducing
//    // 4) throttle demand > 90%
//    // If these four conditions exist simultaneously, then the protection
//    // mode will be activated.
//    // Once active, the following condition are required to stay in the mode
//    // 1) Underspeed protection not active
//    // 2) Specific total energy error > 0
//    // This mode will produce an undulating speed and height response as it cuts in and out but will prevent the aircraft from descending into the ground if an unachievable speed demand is set
//    float STEdot = _SPEdot + _SKEdot;
//    if ((!_flags.underspeed && (_STE_error > 200.0f) && (STEdot < 0.0f) && (_throttle_dem >= _THRmaxf * 0.9f)) || (_flags.badDescent && !_flags.underspeed && (_STE_error > 0.0f)))
//    {
//        _flags.badDescent = true;
//    }
//    else
//    {
//        _flags.badDescent = false;
//    }
//
//    // when soaring is active we never trigger a bad descent
//    if (_soaring_controller.is_active() && _soaring_controller.get_throttle_suppressed()) {
//        _flags.badDescent = false;        
//    }
//}