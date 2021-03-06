#include "fusion_ekf.h"
#include "tools.h"
#include "Eigen/Dense"
#include <iostream>

using namespace std;
using Eigen::MatrixXd;
using Eigen::VectorXd;
using std::vector;

/*
 * Constructor.
 */
FusionEKF::FusionEKF() {
  is_initialized_ = false;

  previous_timestamp_ = 0;

  // initializing matrices
  //measurement covariance matrix - laser
  R_laser_ = MatrixXd(2, 2).setZero();
  R_laser_(0, 0) = 0.0225;
  R_laser_(1, 1) = 0.0225;

  //measurement covariance matrix - radar
  R_radar_ = MatrixXd(3, 3).setZero();
  R_radar_(0, 0) = 0.09;
  R_radar_(1, 1) = 0.0009;
  R_radar_(2, 2) = 0.09;

  H_laser_ = MatrixXd(2, 4).setZero();
  H_laser_(0, 0) = 1;
  H_laser_(1, 1) = 1;

  Hj_ = MatrixXd(3, 4).setZero();

  /**
  TODO:
    * Finish initializing the FusionEKF.
    * Set the process and measurement noises
  */
}

/**
* Destructor.
*/
FusionEKF::~FusionEKF() {}

void FusionEKF::ProcessMeasurement(const MeasurementPackage &measurement_pack) {


  /*****************************************************************************
   *  Initialization
   ****************************************************************************/
  if (!is_initialized_) {
    /**
    TODO:
      * Initialize the state ekf_.x_ with the first measurement.
      * Create the covariance matrix.
      * Remember: you'll need to convert radar from polar to cartesian coordinates.
    */
    // first measurement
    cout << "EKF: " << endl;
    ekf_.x_ = VectorXd(4);
    ekf_.x_ << 1, 1, 1, 1;

    if (measurement_pack.sensor_type_ == MeasurementPackage::RADAR) {
      /**
      Convert radar from polar to cartesian coordinates and initialize state.
      */
        float rho     = measurement_pack.raw_measurements_(0);
        float theta   = measurement_pack.raw_measurements_(1);
        float rho_dot = measurement_pack.raw_measurements_(2);

        ekf_.x_ << rho * cos(theta),    /* px */
                   rho * sin(theta),    /* py */
                   rho_dot * cos(theta) - rho * sin(theta), /* vx */
                   rho_dot * sin(theta) + rho * cos(theta); /* vy */
    }
    else if (measurement_pack.sensor_type_ == MeasurementPackage::LASER) {
      /**
      Initialize state.
      */
        ekf_.x_ << measurement_pack.raw_measurements_(0),
                   measurement_pack.raw_measurements_(1),
                   0.,
                   0.;
    }

    // done initializing, no need to predict or update
    previous_timestamp_ = measurement_pack.timestamp_;
    is_initialized_ = true;
    return;
  }

  /*****************************************************************************
   *  Prediction
   ****************************************************************************/

  /**
   TODO:
     * Update the state transition matrix F according to the new elapsed time.
      - Time is measured in seconds.
     * Update the process noise covariance matrix.
     * Use noise_ax = 9 and noise_ay = 9 for your Q matrix.
   */
    float dt = 1e-6*(measurement_pack.timestamp_ - previous_timestamp_);
    ekf_.F_ << 1, 0, dt, 0,
               0, 1, 0, dt,
               0, 0, 1, 0,
               0, 0, 0, 1;

    // Compute process noise covariance matrix
    float dt_2 = dt * dt;
    float dt_3 = dt_2 * dt;
    float dt_4 = dt_3 * dt;
    float noise_ax = 9;
    float noise_ay = 9;
    ekf_.Q_	<<	dt_4/4 * noise_ax,	0,	dt_3 / 2 * noise_ax, 0,
                0,dt_4 / 4 * noise_ay,	0, dt_3 / 2 * noise_ay,
                dt_3/2 * noise_ax,	0,	dt_2 * noise_ax, 0,
                0, dt_3 / 2 * noise_ay, 0, dt_2 * noise_ay;

    ekf_.Predict();

  /*****************************************************************************
   *  Update
   ****************************************************************************/

  /**
   TODO:
     * Use the sensor type to perform the update step.
     * Update the state and covariance matrices.
   */

  if (measurement_pack.sensor_type_ == MeasurementPackage::RADAR) {
    // Radar updates
    ekf_.H_ = tools.CalculateJacobian(ekf_.x_);
    ekf_.R_ = R_radar_;
    ekf_.UpdateRadar(measurement_pack.raw_measurements_);
  } else {
    // Laser updates
    ekf_.H_ = H_laser_;
    ekf_.R_ = R_laser_;
    ekf_.UpdateLidar(measurement_pack.raw_measurements_);
  }

  previous_timestamp_ = measurement_pack.timestamp_;

  // print the output
  cout << "    ------------    "  << endl;
  cout << "x_ = "  << endl << ekf_.x_ << endl;
  cout << "P_ = "  << endl << ekf_.P_ << endl << endl;
}
