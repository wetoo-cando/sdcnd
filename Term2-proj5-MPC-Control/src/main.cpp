#include <math.h>
#include <uWS/uWS.h>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>
#include "Eigen-3.3/Eigen/Core"
#include "Eigen-3.3/Eigen/QR"
#include "MPC.h"
#include "json.hpp"

// for convenience
using json = nlohmann::json;

// For converting back and forth between radians and degrees.
constexpr double pi() { return M_PI; }
double deg2rad(double x) { return x * pi() / 180; }
double rad2deg(double x) { return x * 180 / pi(); }

/* Checks if the SocketIO event has JSON data.
   If there is data the JSON object in string format will be returned,
   else the empty string "" will be returned. */
string hasData(string s) {
  auto found_null = s.find("null");
  auto b1 = s.find_first_of("[");
  auto b2 = s.rfind("}]");
  if (found_null != string::npos) {
    return "";
  } else if (b1 != string::npos && b2 != string::npos) {
    return s.substr(b1, b2 - b1 + 2);
  }
  return "";
}

// Evaluate a polynomial.
double polyeval(Eigen::VectorXd coeffs, double x) {
  double result = 0.0;
  for (int i = 0; i < coeffs.size(); i++) {
    result += coeffs[i] * pow(x, i);
  }
  return result;
}

/* Fit a polynomial.
https://github.com/JuliaMath/Polynomials.jl/blob/master/src/Polynomials.jl#L676-L716 */
Eigen::VectorXd polyfit(Eigen::VectorXd xvals, Eigen::VectorXd yvals,
                        int order) {
  assert(xvals.size() == yvals.size());
  assert(order >= 1 && order <= xvals.size() - 1);
  Eigen::MatrixXd A(xvals.size(), order + 1);

  for (int i = 0; i < xvals.size(); i++) {
    A(i, 0) = 1.0;
  }

  for (int j = 0; j < xvals.size(); j++) {
    for (int i = 0; i < order; i++) {
      A(j, i + 1) = A(j, i) * xvals(j);
    }
  }

  auto Q = A.householderQr();
  auto result = Q.solve(yvals);
  return result;
}

int main() {
  uWS::Hub h;

  MPC mpc;
  h.onMessage([&mpc](uWS::WebSocket<uWS::SERVER> ws, char *data, size_t length,
                     uWS::OpCode opCode) {
    /* "42" at the start of the message means there's a websocket message event.
       The 4 signifies a websocket message
       The 2 signifies a websocket event */
    string sdata = string(data).substr(0, length);
    cout << endl << sdata << endl;
    if (sdata.size() > 2 && sdata[0] == '4' && sdata[1] == '2') {
      string s = hasData(sdata);
      if (s != "") {
        auto j = json::parse(s);
        string event = j[0].get<string>();
        if (event == "telemetry") {
          // j[1] is the data JSON object
          vector<double> ptsx = j[1]["ptsx"];
          vector<double> ptsy = j[1]["ptsy"];
          double px = j[1]["x"];
          double py = j[1]["y"];
          double psi = j[1]["psi"];
          double v = j[1]["speed"];
          double delta = j[1]["steering_angle"];
          double a = j[1]["throttle"];

          // Transform waypoints to car reference frame
          for (size_t i = 0; i < ptsx.size(); ++i) {
            // Shift origin to car frame origin
            double x = ptsx[i] - px;
            double y = ptsy[i] - py;

            // Rotate points in car frame
            ptsx[i] = x * cos(-psi) - y * sin(-psi);
            ptsy[i] = x * sin(-psi) + y * cos(-psi);
          }

          // Convert to Eigen::VectorXd
          size_t num_waypoints = 6;
          double *ptrx = &ptsx[0];
          Eigen::Map<Eigen::VectorXd> ptsx_transform(ptrx, num_waypoints);

          double *ptry = &ptsy[0];
          Eigen::Map<Eigen::VectorXd> ptsy_transform(ptry, num_waypoints);

          Eigen::VectorXd coeffs = polyfit(ptsx_transform, ptsy_transform, 3);
          double cte = polyeval(coeffs, 0);

          // Evaluate 3rd order polynomial derivative at x = 0
          double epsi = -atan(coeffs[1]);

          // Account for latency between command and response
          const double dt = 0.1;  // assume 100 ms latency
          const double Lf = 2.67;
          // x, y and psi = 0 in car frame
          double late_px = 0.0 + v * dt;
          double late_py  = 0.0;
          double late_psi = 0.0 + v/Lf * -delta * dt;
          double late_v = v + a * dt;
          double late_cte = cte + v * sin(epsi) * dt;
          double late_epsi = epsi + v/Lf * -delta * dt;

          Eigen::VectorXd state(6);
          //state << 0, 0, 0, v, cte, epsi;
          state << late_px, late_py, late_psi, late_v, late_cte, late_epsi;

          std::vector<double> solution = mpc.Solve(state, coeffs);

          json msgJson;
          /* NOTE: Remember to divide by deg2rad(25) before you send the steering value back.
          Otherwise the values will be in between [-deg2rad(25), deg2rad(25] instead of [-1, 1]. */
          msgJson["steering_angle"] = solution[0]/(Lf*deg2rad(25));
          msgJson["throttle"] = solution[1];

          // Display the MPC predicted trajectory
          vector<double> mpc_x_vals = {state[0]};
          vector<double> mpc_y_vals = {state[1]};

          /* Add (x,y) points to list here, points are w.r.t vehicle's coordinate system.
          The points in the simulator are connected by a Green line. */
          for (size_t i = 2; i < solution.size(); i+=2) {
           mpc_x_vals.push_back(solution[i]);
           mpc_y_vals.push_back(solution[i+1]);
          }

          msgJson["mpc_x"] = mpc_x_vals;
          msgJson["mpc_y"] = mpc_y_vals;

          // Display the waypoints/reference line
          vector<double> next_x_vals;
          vector<double> next_y_vals;

          /* Add waypoints points here (w.r.t vehicle's coordinate system).
          The points get connected in the simulator by a Yellow line. */
          for (size_t i = 0; i < ptsx.size(); ++i) {
            next_x_vals.push_back(ptsx[i]);
            next_y_vals.push_back(ptsy[i]);
          }

          msgJson["next_x"] = next_x_vals;
          msgJson["next_y"] = next_y_vals;

          auto msg = "42[\"steer\"," + msgJson.dump() + "]";
          std::cout << msg << std::endl;
          /* Latency
           * The purpose is to mimic real driving conditions where
           * the car does NOT actuate the commands instantly.
           *
           * Feel free to play around with this value but should be to drive
           * around the track with 100ms latency.
           *
           * NOTE: REMEMBER TO SET THIS TO 100 MILLISECONDS BEFORE SUBMITTING.
           */
          this_thread::sleep_for(chrono::milliseconds(100));
          ws.send(msg.data(), msg.length(), uWS::OpCode::TEXT);
        }
      } else {
        // Manual driving
        std::string msg = "42[\"manual\",{}]";
        ws.send(msg.data(), msg.length(), uWS::OpCode::TEXT);
      }
    }
  });

  /* We don't need this since we're not using HTTP but if it's removed the
  program doesn't compile :-( */
  h.onHttpRequest([](uWS::HttpResponse *res, uWS::HttpRequest req, char *data,
                     size_t, size_t) {
    const std::string s = "<h1>Hello world!</h1>";
    if (req.getUrl().valueLength == 1) {
      res->end(s.data(), s.length());
    } else {
      // i guess this should be done more gracefully?
      res->end(nullptr, 0);
    }
  });

  h.onConnection([&h](uWS::WebSocket<uWS::SERVER> ws, uWS::HttpRequest req) {
    std::cout << "Connected!!!" << std::endl;
  });

  h.onDisconnection([&h](uWS::WebSocket<uWS::SERVER> ws, int code,
                         char *message, size_t length) {
    ws.close();
    std::cout << "Disconnected" << std::endl;
  });

  int port = 4567;
  if (h.listen(port)) {
    std::cout << "Listening to port " << port << std::endl;
  } else {
    std::cerr << "Failed to listen to port" << std::endl;
    return -1;
  }
  h.run();
}
