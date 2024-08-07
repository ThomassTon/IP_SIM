#ifndef LQR_CONTROLLER_H
#define LQR_CONTROLLER_H

#include <vehicle_controller/controller.h>
#include <vehicle_controller/LqrControllerParamsConfig.h>

#include <vehicle_controller/ekf.h>
#include <narrow_passage_detection_msgs/NarrowPassageController.h>


class Lqr_Controller : public Controller
{
public:

  Lqr_Controller(ros::NodeHandle& nh_);
  ~Lqr_Controller() override;

  bool configure() override;

  inline std::string getName() override
  {
    return "LQR Controller";
  }
void computeMoveCmd_mpc(double &angular_vel_, double &lin_vel_dir_);
protected:
  void computeMoveCmd() override;
  void reset() override;
  
  virtual void controllerParamsCallback(vehicle_controller::LqrControllerParamsConfig & config, uint32_t level);

  //lqr control specific functions
  void calc_local_path();
  int calcClosestPoint();
  void calcLqr();
  void solveDare();
  void checkPathReq();

  ros::NodeHandle nh_dr_params;
  dynamic_reconfigure::Server<vehicle_controller::LqrControllerParamsConfig> * dr_controller_params_server;

  ros::Subscriber lqr_params_narrow;
  void lqr_params_callback(const narrow_passage_detection_msgs::NarrowPassageController msg);

  // LQR specific variables
  geometry_msgs::PointStamped closest_point;
  double rot_vel_dir, lin_vel_dir;
  double local_path_radius;
  double alignment_angle;

  double lqr_y_error, lqr_x_error;
  double lqr_angle_error;

  geometry_msgs::Twist lqr_last_cmd;
  double lqr_last_y_error;
  double lqr_last_angle_error;


  double lqr_q11;
  double lqr_q22;
  double lqr_r;

  double lqr_p11, lqr_p12, lqr_p22;
  double lqr_k1, lqr_k2, lqr_k3;

  ros::Time lqr_time;
  Eigen::Matrix<double, 1, 2> K;

};


#endif // LQR_CONTROLLER_H
