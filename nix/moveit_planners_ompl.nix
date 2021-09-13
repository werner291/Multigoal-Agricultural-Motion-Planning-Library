{ pkgs }:
with pkgs; with rosPackages.noetic; buildRosPackage {
  pname = "ros-noetic-moveit-planners-ompl";
  version = "custom-master";

  src = /home/werner/catkin_ws/src/moveit;

  buildType = "catkin";
  propagatedBuildInputs = [ dynamic-reconfigure
        (import ./moveit_core.nix {pkgs=pkgs;})
        (import ./moveit_ros_planning.nix {pkgs=pkgs;})
        ompl
        pluginlib
        rosconsole
        roscpp
        tf2 ];
  nativeBuildInputs = [ catkin ];

  configurePhase = ''
              cmake moveit_planners/ompl -DCMAKE_INSTALL_PREFIX=$out
          '';

  meta = {
    description = ''MoveIt interface to OMPL'';
    license = with lib.licenses; [ bsdOriginal ];
  };
}