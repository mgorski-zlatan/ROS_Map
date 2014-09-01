#include <ros/ros.h>
#include <tf/transform_broadcaster.h>
#include <nav_msgs/Odometry.h>
#include <trobot/Odometry.h>

double vx = 0.0;
double vy = 0.0;
double vth = 0.0;

double wheelDistance;
double axialDistance;
double alpha;

void getOdometry(const trobot::Odometry::ConstPtr& msg)
{
  ROS_INFO("I heard rwheel: %f", msg->rightWheelSpeed);
  ROS_INFO("I heard lwheel: %f", msg->leftWheelSpeed);

  vx = (msg->rightWheelSpeed + msg->leftWheelSpeed) / 2;
  vth = (4 * alpha * (msg->rightWheelSpeed - vx)) / (2 * wheelDistance + axialDistance);
}

void setupParameters()
{
  ros::NodeHandle n("~");

  n.param("wheel_distance", wheelDistance, 1.0);
  n.param("axial_distance", axialDistance, 1.0);
  n.param("alpha", alpha, 1.0);
}

int main(int argc, char** argv){
  ros::init(argc, argv, "odometry_publisher");

  setupParameters();

  ros::NodeHandle n;
  ros::Subscriber sub = n.subscribe("RoboteQNode/speed", 1, getOdometry);
  ros::Publisher odom_pub = n.advertise<nav_msgs::Odometry>("OdometryNode/odom", 50);
  tf::TransformBroadcaster odom_broadcaster;

  double x = 0.0;
  double y = 0.0;
  double th = 0.0;

  ros::Time current_time, last_time;
  current_time = ros::Time::now();
  last_time = ros::Time::now();

  ros::Rate r(10);
  while(n.ok()){

    ros::spinOnce();               // check for incoming messages
    current_time = ros::Time::now();

    //compute odometry in a typical way given the velocities of the robot
    double dt = (current_time - last_time).toSec();
    double delta_x = (vx * cos(th) - vy * sin(th)) * dt;
    double delta_y = (vx * sin(th) + vy * cos(th)) * dt;
    double delta_th = vth * dt;

    x += delta_x;
    y += delta_y;
    th += delta_th;

    //since all odometry is 6DOF we'll need a quaternion created from yaw
    geometry_msgs::Quaternion odom_quat = tf::createQuaternionMsgFromYaw(th);

    //first, we'll publish the transform over tf
    geometry_msgs::TransformStamped odom_trans;
    odom_trans.header.stamp = current_time;
    odom_trans.header.frame_id = "odom";
    odom_trans.child_frame_id = "base_footprint";

    odom_trans.transform.translation.x = x;
    odom_trans.transform.translation.y = y;
    odom_trans.transform.translation.z = 0.0;
    odom_trans.transform.rotation = odom_quat;

    //send the transform
    //odom_broadcaster.sendTransform(odom_trans);	// robot_pose_efk is publishing odom tf

    //next, we'll publish the odometry message over ROS
    nav_msgs::Odometry odom;
    odom.header.stamp = current_time;
    odom.header.frame_id = "odom";

    //set the position
    odom.pose.pose.position.x = x;
    odom.pose.pose.position.y = y;
    odom.pose.pose.position.z = 0.0;
    odom.pose.pose.orientation = odom_quat;

    odom.pose.covariance[0] = 0.00001;
    odom.pose.covariance[7] = 0.00001;
    odom.pose.covariance[14] = 1000000000000.0;
    odom.pose.covariance[21] = 1000000000000.0;
    odom.pose.covariance[28] = 1000000000000.0;
    odom.pose.covariance[35] = 0.001;

    //set the velocity
    odom.child_frame_id = "base_footprint";
    odom.twist.twist.linear.x = vx;
    odom.twist.twist.linear.y = vy;
    odom.twist.twist.angular.z = vth;

    odom.twist.covariance = odom.pose.covariance;

    //publish the message
    odom_pub.publish(odom);

    last_time = current_time;
    r.sleep();
  }
}