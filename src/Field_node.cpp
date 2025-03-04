#include "Field_node.hpp"

FieldNode::FieldNode(ros::NodeHandle *nh) {
    // ROSPARAMS here
    if (!ros::param::get("~xNum", xNum)) {  // number of. PSU for X axis
        this->xNum = 2;
    }

    if (!ros::param::get("~yNum", yNum)) {  // number of. PSU for Y axis
        this->yNum = 2;
    }

    if (!ros::param::get("~zNum", zNum)) {  // number of. PSU for Z axis
        this->zNum = 2;
    }

    if (!ros::param::get("~xRoot", xRoot)) {  // first address of. PSU for X axis
        this->xRoot = "/PSU0";
    }

    if (!ros::param::get("~yRoot", yRoot)) {  // first address of. PSU for Y axis
        this->yRoot = "/PSU4";
    }

    if (!ros::param::get("~zRoot", zRoot)) {  // first address of. PSU for Z axis
        this->zRoot = "/PSU2";
    }

    for (int i = 0; i < xNum; i++) {
        std::string rootDuplicate = xRoot;
        char num = xRoot[4];
        num += i;
        rootDuplicate[4] = num;
        rootDuplicate = "/vi_control" + rootDuplicate;
        xAddress.push_back(rootDuplicate);
    }

    for (int i = 0; i < yNum; i++) {
        std::string rootDuplicate = yRoot;
        char num = yRoot[4];
        num += i;
        rootDuplicate[4] = num;
        rootDuplicate = "/vi_control" + rootDuplicate;
        yAddress.push_back(rootDuplicate);
    }

    for (int i = 0; i < zNum; i++) {
        std::string rootDuplicate = zRoot;
        char num = zRoot[4];
        num += i;
        rootDuplicate[4] = num;
        rootDuplicate = "/vi_control" + rootDuplicate;
        zAddress.push_back(rootDuplicate);
    }
    // Variable initialisation here
    allAddress.insert(allAddress.end(), xAddress.begin(), xAddress.end());
    allAddress.insert(allAddress.end(), yAddress.begin(), yAddress.end());
    allAddress.insert(allAddress.end(), zAddress.begin(), zAddress.end());
    int advNum = allAddress.size();  // number of advertise calls required
    vi_.resize(advNum);
    // Subscriber instantiation here
    field_Subscriber_ =
        nh->subscribe("field", 10, &FieldNode::callbackField, this);
    this->bx = 0;
    this->by = 0;
    this->bz = 0;
}

void FieldNode::callbackField(const ros_coils::magField &msg) {
    ROS_INFO("Field: %f, %f, %f", msg.bx, msg.by, msg.bz);

    try {
        bool bx_change = abs(msg.bx - this->bx) > this->maxChange;
        bool by_change = abs(msg.by - this->by) > this->maxChange;
        bool bz_change = abs(msg.bz - this->bz) > this->maxChange;
        if (bx_change) {
            throw ros_coils::ChangeException(
                "Change in x field exceeds boundary of 15mT, exiting. Please restart the "
                "node");
        }
        if (by_change) {
            throw ros_coils::ChangeException(
                "Change in y field  exceeds boundary of 15mT, exiting. Please restart the node");
        }
        if (bz_change) {
            throw ros_coils::ChangeException(
                "Change in z field  exceeds boundary of 15mT, exiting. Please restart the node");
        }
        if(abs(msg.bx) > this->maxField){
            throw ros_coils::maxFieldException(
                "Field in x axis  exceeds boundary of 22mT, exiting. Please restart the node");
        }
        if(abs(msg.by) > this->maxField){
            throw ros_coils::maxFieldException(
                "Field in y axis  exceeds boundary of 22mT, exiting. Please restart the node");
        }
        if(abs(msg.bz) > this->maxField){
            throw ros_coils::maxFieldException(
                "Field in z axis exceeds boundary of 22mT, exiting. Please restart the node");
        }
    } catch (ros_coils::ChangeException &e) {
        ROS_ERROR("ChangeException: %s", e.what());
        ros::shutdown();
    } catch (ros_coils::maxFieldException &e) {
        ROS_ERROR("maxFieldException: %s", e.what());
        ros::shutdown();
    }


    this->bx = msg.bx;
    this->by = msg.by;
    this->bz = msg.bz;
    this->field_to_vi();
}

/**
 * @brief uses self.bx, self.by, self.bz to. Populate self.vi_
 *
 */
void FieldNode::field_to_vi() {
    float ix = bx / cal_x;
    float iy = by / cal_y;
    float iz = bz / cal_z;

    for (size_t i = 0; i < xNum; i++) {
        vi_[i].I = ix;
        vi_[i].V = std::min((abs(ix)<3?ix*1.8:ix*1.2),50.0);
        // vi_[i].V = abs(ix);
        if (xNum != 2) {
            vi_[i].V *= 1.2;
        }
    }
    for (size_t i = 0; i < yNum; i++) {
        vi_[i + xNum].I = iy;
        vi_[i + xNum].V = std::min((abs(ix)<3?ix*1.8:ix*1.2),50.0);
        // vi_[i + xNum].V = abs(iy);
        if (yNum != 2) {
            vi_[i + xNum].V *= 1.2;
        }
        // if(i == 0) vi_[i + xNum].I *= -1;
    }
    for (size_t i = 0; i < zNum; i++) {
        vi_[i + xNum + yNum].I = iz;
        vi_[i + xNum + yNum].V = std::min((abs(iz)<3?iz*1.8:iz*1.2),50.0);
        // vi_[i + xNum + yNum].V = abs(iz);
        if (zNum != 2) {
            vi_[i + xNum + yNum].V *= 1.2;
        }
    }
}

int main(int argc, char *argv[]) {
    ros::init(argc, argv, "FieldNode");
    ros::NodeHandle nh;
    FieldNode field(&nh);

    std::vector<std::string> allAddress = field.allAddress;
    int advNum = allAddress.size();  // number of advertise calls required
    float bx, by, bz;

    std::vector<ros::Publisher> viPub_;
    std::vector<ros_coils::VI> vi_(advNum);
    viPub_.resize(advNum);
    //. Publisher instantiation here
    for (size_t i = 0; i < advNum; i++) {
        viPub_[i] = nh.advertise<ros_coils::VI>(allAddress[i], 10);
    }

    ros::AsyncSpinner spinner(6);
    spinner.start();
    ros::Duration freq(0.5);
    while (ros::ok()) {
        if (vi_ == field.vi_) {
            // ROS_INFO("Input vi array is equal as held value. not inputting");
        } else {
            vi_ = field.vi_;
            for (size_t i = 0; i < advNum; i++) {
                viPub_[i].publish(vi_[i]);
            }
        }
        freq.sleep();
    }

    spinner.stop();
    return 1;
}
