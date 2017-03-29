#include <gtest/gtest.h>

#include <cstring>

#include <robot_impl.h>

#include "mock_server.h"
#include "helpers.h"

using franka::RobotState;
using franka::NetworkException;
using franka::MotionGeneratorException;
using MotionGeneratorType = research_interface::StartMotionGeneratorRequest::Type;
using namespace std::chrono_literals;

class Robot : public ::franka::Robot {
 public:
   using ::franka::Robot::Impl;
};

TEST(Robot, ThrowsTimeoutIfNoRobotStateArrives) {
  research_interface::RobotState sent_robot_state;
  randomRobotState(sent_robot_state);

  MockServer server;
  server.spinOnce();

  Robot::Impl robot("127.0.0.1", research_interface::kCommandPort, 1ms);

  ASSERT_THROW(robot.update(), NetworkException);
}

TEST(Robot, StopsIfControlConnectionClosed) {
  std::unique_ptr<Robot::Impl> robot;
  {
    MockServer server;
    server
      .sendEmptyRobotState()
      .spinOnce();

    robot.reset(new Robot::Impl("127.0.0.1", research_interface::kCommandPort, 1ms));

    EXPECT_TRUE(robot->update());
  }

  EXPECT_FALSE(robot->update());
}

TEST(Robot, CanStartMotionGenerator) {
  MockServer server;
  server
    .onStartMotionGenerator([](const research_interface::StartMotionGeneratorRequest request) {
      EXPECT_EQ(research_interface::StartMotionGeneratorRequest::Type::kJointVelocity, request.type);
      return research_interface::StartMotionGeneratorReply(research_interface::StartMotionGeneratorReply::Status::kSuccess);
    })
    .onSendRobotState([](research_interface::RobotState& robot_state) {
      robot_state.motion_generator_mode = research_interface::MotionGeneratorMode::kJointVelocity;
    })
    .spinOnce();

  Robot::Impl robot("127.0.0.1");
  EXPECT_NO_THROW(robot.startMotionGenerator(MotionGeneratorType::kJointVelocity));
}

TEST(Robot, CanNotStartMultipleMotionGenerators) {
  MockServer server;
  server
    .onStartMotionGenerator([](const research_interface::StartMotionGeneratorRequest request) {
      EXPECT_EQ(research_interface::StartMotionGeneratorRequest::Type::kJointPosition, request.type);
      return research_interface::StartMotionGeneratorReply(research_interface::StartMotionGeneratorReply::Status::kSuccess);
    })
    .onSendRobotState([](research_interface::RobotState& robot_state) {
      robot_state.motion_generator_mode = research_interface::MotionGeneratorMode::kJointPosition;
    })
    .spinOnce();

  Robot::Impl robot("127.0.0.1");
  robot.startMotionGenerator(MotionGeneratorType::kJointPosition);
  EXPECT_THROW(robot.startMotionGenerator(MotionGeneratorType::kJointVelocity), MotionGeneratorException);
}

TEST(Robot, CanSendMotionGeneratorCommand) {
  research_interface::RobotCommand sent_command;
  randomRobotCommand(sent_command);
  sent_command.motion.motion_generation_finished = false;

  MockServer server;
  server
    .onStartMotionGenerator([](const research_interface::StartMotionGeneratorRequest request) {
      EXPECT_EQ(research_interface::StartMotionGeneratorRequest::Type::kCartesianPosition, request.type);
      return research_interface::StartMotionGeneratorReply(research_interface::StartMotionGeneratorReply::Status::kSuccess);
    })
    .onSendRobotState([](research_interface::RobotState& robot_state) {
      robot_state.motion_generator_mode = research_interface::MotionGeneratorMode::kCartesianPosition;
    })
    .spinOnce();

  Robot::Impl robot("127.0.0.1");
  robot.startMotionGenerator(MotionGeneratorType::kCartesianPosition);

  robot.motionCommand() = sent_command.motion;

  server
    .onSendRobotState([](research_interface::RobotState& robot_state) {
      robot_state.motion_generator_mode = research_interface::MotionGeneratorMode::kCartesianPosition;
    })
    .onReceiveRobotCommand([&](const research_interface::RobotCommand& command) {
      testMotionGeneratorCommandsAreEqual(sent_command.motion, command.motion);
    })
    .spinOnce();

  EXPECT_TRUE(robot.update());
  EXPECT_TRUE(robot.motionGeneratorRunning());
}

TEST(Robot, CanReceiveMotionGenerationError) {
  MockServer server;
  server
    .onStartMotionGenerator([](const research_interface::StartMotionGeneratorRequest request) {
      EXPECT_EQ(research_interface::StartMotionGeneratorRequest::Type::kCartesianPosition, request.type);
      return research_interface::StartMotionGeneratorReply(research_interface::StartMotionGeneratorReply::Status::kSuccess);
    })
    .onSendRobotState([](research_interface::RobotState& robot_state) {
      robot_state.motion_generator_mode = research_interface::MotionGeneratorMode::kCartesianPosition;
    })
    .spinOnce();

  Robot::Impl robot("127.0.0.1");
  robot.startMotionGenerator(MotionGeneratorType::kCartesianPosition);

  server
    .onSendRobotState([](research_interface::RobotState& robot_state) {
      robot_state.motion_generator_mode = research_interface::MotionGeneratorMode::kCartesianPosition;
    })
    .onReceiveRobotCommand([](const research_interface::RobotCommand&) {
    })
    .spinOnce();

  EXPECT_TRUE(robot.update());

  server
    .sendReply<research_interface::StartMotionGeneratorReply>([]() {
      return research_interface::StartMotionGeneratorReply(research_interface::StartMotionGeneratorReply::Status::kRejected);
    })
    .spinOnce(/* block until reply has been sent */ true);

  EXPECT_THROW(robot.update(), MotionGeneratorException);
  EXPECT_FALSE(robot.motionGeneratorRunning());
}

TEST(Robot, CanStopMotionGenerator) {
  MockServer server;
  server
    .onStartMotionGenerator([](const research_interface::StartMotionGeneratorRequest request) {
      EXPECT_EQ(research_interface::StartMotionGeneratorRequest::Type::kCartesianVelocity, request.type);
      return research_interface::StartMotionGeneratorReply(research_interface::StartMotionGeneratorReply::Status::kSuccess);
    })
    .onSendRobotState([](research_interface::RobotState& robot_state) {
      robot_state.motion_generator_mode = research_interface::MotionGeneratorMode::kCartesianVelocity;
    })
    .spinOnce();

  Robot::Impl robot("127.0.0.1");
  robot.startMotionGenerator(MotionGeneratorType::kCartesianVelocity);

  server
    .onSendRobotState([](research_interface::RobotState& robot_state) {
      robot_state.motion_generator_mode = research_interface::MotionGeneratorMode::kCartesianPosition;
    })
    .onReceiveRobotCommand([](const research_interface::RobotCommand&) {
    })
    .spinOnce();

  EXPECT_TRUE(robot.update());

  server
    .onStopMotionGenerator([](const research_interface::StopMotionGeneratorRequest) {
      return research_interface::StopMotionGeneratorReply(research_interface::StopMotionGeneratorReply::Status::kSuccess);
    })
    .sendEmptyRobotState()
    .spinOnce();

  robot.stopMotionGenerator();

  server
    .sendEmptyRobotState()
    .onReceiveRobotCommand([](const research_interface::RobotCommand&) {
    })
    .spinOnce();

  EXPECT_TRUE(robot.update());
  EXPECT_FALSE(robot.motionGeneratorRunning());
}