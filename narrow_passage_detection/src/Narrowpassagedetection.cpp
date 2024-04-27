#include"Narrowpassagedetection.hpp"
#include <unistd.h>


namespace narrow_passage_detection{

    bool show=false;

    Narrowpassagedetection::Narrowpassagedetection(ros::NodeHandle& nodeHandle):nh(nodeHandle){
        
        nh.setCallbackQueue(&queue_1);
        map_sub = nh.subscribe("/elevation_mapping/elevation_map_raw",1, &Narrowpassagedetection::map_messageCallback,this);
        extend_point_pub = nh.advertise<geometry_msgs::PoseStamped>("/move_base/narrow_goal",1);
        nh.setCallbackQueue(&queue_2);

        path_sub = nh.subscribe("/smooth_path",1,&Narrowpassagedetection::path_messageCallback, this);
        pose_sub = nh.subscribe("/odom",1,&Narrowpassagedetection::pose_messageCallback,this);
        vel_sub = nh.subscribe("/cmd_vel_raw",1,&Narrowpassagedetection::vel_messageCallback,this);
        map_sub2 = nh.subscribe("/map",1,&Narrowpassagedetection::map_messageCallback2,this);

        map_pub = nh.advertise<grid_map_msgs::GridMap>("/narrow_passage_map", 1);
        width_pub = nh.advertise<std_msgs::String>("/passage_width",1);
        approach_distacne_pub = nh.advertise<narrow_passage_detection_msgs::NarrowPassage>("/narrow_passage_approach",1);

        // nh.setCallbackQueue(&queue_3);

        // path_sub = nh.subscribe("/smooth_path",1,&Narrowpassagedetection::path_messageCallback, this);

        // maxduration.fromSec(1.00);


        // mapUpdateTimer_ = nh.createTimer(maxduration, &Narrowpassagedetection::mapUpdateTimerCallback, this, false, false);
        // mapUpdateTimer_.start();
        // setupTimers();
        // initialize();
    }

    void Narrowpassagedetection::map_messageCallback2(const nav_msgs::OccupancyGrid& msg){
        grid_map::GridMapRosConverter::fromOccupancyGrid(msg, std::string("occupancy"), occupancy_map);

    }
    void Narrowpassagedetection::path_messageCallback(const nav_msgs::Path& msg){
        // std::cout<<msg.poses[0]<<std::endl;

        // for(int i =0;i<msg.poses.size();)
        // {

        // }
        path_msg = msg;
        get_path = true;            

    }

    void Narrowpassagedetection::map_messageCallback(const grid_map_msgs::GridMap& msg)
    {

    // ROS_INFO("Received message: \n\n\n\n\n\n\n\n\n\n\n\n");
        
        grid_map::GridMapRosConverter::fromMessage(msg, elevationmap_);
        getmap = true;
        outputmap = elevationmap_;
        outputmap2 = elevationmap_;
        grid_data = outputmap["elevation"];

        bool result = grid_map::GridMapCvConverter::toImage<unsigned char, 1>(elevationmap_, "elevation", CV_8UC1, input_img);
        computegradient();
        outputmap.add("elevation",grid_data);
        outputmap2.add("elevation",grid_data);

        if(result){
            ros::Time start_time = ros::Time::now();
            bool isSuccess;
            grid_map::GridMap map;
            grid_map::Length length(2.5,2.5);            
            grid_map::Length length2(2,2);

            if(get_path){
                // int max_index=0;
                // if(path_msg.poses.size()>50){
                //     max_index=48;
                // }
                // else{
                //     max_index=path_msg.poses.size()-4; ///-2
                // }
                // std::cout<<path_msg.poses[max_index]<<std::endl;
                // int index=max_index;
                // ROS_INFO("test11111\n\n\n\n");

                int index = get_path_index(path_msg, 2.0);
                tf::Quaternion quat;
                tf::quaternionMsgToTF(path_msg.poses[index].pose.orientation, quat);
                double roll_, pitch_, yaw_;
                tf::Matrix3x3(quat).getRPY(roll_,pitch_,yaw_);
                grid_map::Position robot_position2(path_msg.poses[index].pose.position.x,path_msg.poses[index].pose.position.y);
                map = outputmap2.getSubmap(robot_position2,length2, isSuccess);
                
                bool narrow = generate_output2(path_msg.poses[index].pose.position.x, path_msg.poses[index].pose.position.y, yaw_,map, mid_pose);
                if(narrow){
                    geometry_msgs::Pose extend_pose= extend_point(mid_pose, 0.45);
                    extend_point_publisher(extend_pose);
                    narrow_passage_dectected = true;

                }
                // std::cout<<robot_yaw/M_PI*180.0 <<"     "<<yaw_/M_PI*180.0<<"\n\n\n\n"<<std::endl;
                get_path = false;  

            }

            grid_map::Position robot_position(robot_pose_msg.pose.pose.position.x,robot_pose_msg.pose.pose.position.y);
            map = outputmap.getSubmap(robot_position,length, isSuccess);
            generate_output(robot_pose_msg.pose.pose.position.x, robot_pose_msg.pose.pose.position.y, robot_yaw,map);

            ros::Time end_time = ros::Time::now();
            ros::Duration duration = end_time - start_time;

    // // 输出时间差
            ROS_INFO("Time elapsed: %.3f seconds", duration.toSec());   
            narrowmap_pub(outputmap);
     
        }


    }

    void Narrowpassagedetection::pose_messageCallback(const nav_msgs::Odometry &pose)
    {
        robot_pose_msg = pose;
        tf::Quaternion quat;
        tf::quaternionMsgToTF(robot_pose_msg.pose.pose.orientation, quat);
         
        tf::Matrix3x3(quat).getRPY(robot_roll,robot_pitch,robot_yaw);
        float distance;
        if(narrow_passage_dectected){
            if(approach_distance(robot_pose_msg, mid_pose, distance,path_msg)){
                narrow_passage_detection_msgs::NarrowPassage msg;
                msg.approachdistance = distance;
                msg.approached = true;
                approach_distacne_pub.publish(msg);
            }
        }

        // std::cout<<"yaw :"<<yaw<<std::endl;
    }

    void Narrowpassagedetection::vel_messageCallback(const geometry_msgs::Twist& msg){
        vel_msg = msg;
        if(vel_msg.linear.x<-0.0002){    // if(backward){
            //     angleRadians = angle/180.00*M_PI + yaw_ - M_PI;
            // }
            // else{
            backward = true;
        }
        if(vel_msg.linear.x>0.0002){
            backward = false;
        }
    }



    void Narrowpassagedetection::narrowmap_pub(grid_map::GridMap map){
        grid_map_msgs::GridMap message;
        grid_map::GridMapRosConverter::toMessage(map, message);
        map_pub.publish(message);
    }

    bool Narrowpassagedetection::generate_output(double pos_x, double pos_y, double yaw_,grid_map::GridMap map, geometry_msgs::Pose &pos){
        create_ray(pos_x, pos_y, yaw_,map);
        compute_passage_width(map, pos);
        return true;
    }
    bool Narrowpassagedetection::generate_output2(double pos_x, double pos_y, double yaw_,grid_map::GridMap map, geometry_msgs::Pose &pos){
        create_ray(pos_x, pos_y, yaw_,map);
        return (compute_passage_width(map, pos));
        // return true;
    }

    void Narrowpassagedetection::adjust_map(grid_map::GridMap &map){

            
    }
    
    
    void Narrowpassagedetection::computegradient(){
        int len1= input_img.rows;
        int len2= input_img.cols;
        // cv::imwrite("/home/yuan/Documents/input.jpg",input_img);

        cv::Mat gradientX(len1,len2,CV_8UC1), gradientY(len1,len2,CV_8UC1),magnitude(len1,len2,CV_8UC1), gaussian_img(len1,len2,CV_8UC1);
        // cv::GaussianBlur(input_img, input_img, cv::Size(3, 3), 0);
        
        cv::Mat kernal_mat2 = (cv::Mat_<float>(3, 3) <<
		1 / 16.0f, 2 / 16.0f, 1 / 16.0f,
		2 / 16.0f, 4 / 16.0f, 2 / 16.0f,
		1 / 16.0f, 2 / 16.0f, 1 / 16.0f);
        cv::filter2D(input_img,gaussian_img,-1,kernal_mat2);


        cv::Mat kernal_mat1 = (cv::Mat_<char>(3, 3) << 0, -1, 0, -1, 5, -1, 0, -1, 0);
        cv::filter2D(gaussian_img,gaussian_img,-1,kernal_mat1);int len1= input_img.rows;
        int len2= input_img.cols;
        // cv::imwrite("/home/yuan/Documents/input.jpg",input_img);

        cv::Mat gradientX(len1,len2,CV_8UC1), gradientY(len1,len2,CV_8UC1),magnitude(len1,len2,CV_8UC1), gaussian_img(len1,len2,CV_8UC1);
        // cv::GaussianBlur(input_img, input_img, cv::Size(3, 3), 0);
        
        cv::Mat kernal_mat2 = (cv::Mat_<float>(3, 3) <<
		1 / 16.0f, 2 / 16.0f, 1 / 16.0f,
		2 / 16.0f, 4 / 16.0f, 2 / 16.0f,
		1 / 16.0f, 2 / 16.0f, 1 / 16.0f);
        cv::filter2D(input_img,gaussian_img,-1,kernal_mat2);


        cv::Mat kernal_mat1 = (cv::Mat_<char>(3, 3) << 0, -1, 0, -1, 5, -1, 0, -1, 0);
        cv::filter2D(gaussian_img,gaussian_img,-1,kernal_mat1);


        cv::Sobel(gaussian_img, gradientX, CV_32F, 1, 0, 3);
        cv::Sobel(gaussian_img, gradientY, CV_32F, 0, 1, 3);
        cv::cartToPolar(gradientX, gradientY, gradient, direction, true);
        // std::cout<<"compute "<<"\n\n\n\n\n\n\n\n\n"<<std::endl;
        gradient.convertTo(gradient,CV_8UC1);




        for(int i=0;i<gradient.rows;i++)  
        {  
            for(int j=0;j<gradient.cols;j++)  
            {  
                //magnitude.at<Vec3b>(i,j)[0]=magnitude.at<Vec3b>(i,j)[0]/div*div+div/2;  
                // std::cout<<magnitude.at<uchar>(i,j)<<std::endl;
                if(gradient.at<uchar>(i,j)< uchar(255))
                {
                    gradient.at<uchar>(i,j)=uchar(0);
                }
                else{
                    gradient.at<uchar>(i,j) = uchar(255);
                }
            }  
        }
        convert_from_gradient();


    }

    void Narrowpassagedetection::convert_from_gradient(){
        for (grid_map::GridMapIterator iterator(elevationmap_);!iterator.isPastEnd(); ++iterator ){
            const grid_map::Index index(*iterator);
            const float& value = grid_data(index(0), index(1));
            const grid_map::Index imageIndex(iterator.getUnwrappedIndex());
            const float maxValue = elevationmap_.get("elevation").maxCoeffOfFinites();
            if(gradient.at<uchar>(imageIndex(0),imageIndex(1))== uchar(0)&&std::isfinite(value)&&value<0.7*maxValue){
                grid_data(index(0), index(1)) = NAN;
            }          
            // else if(grid_data(index(0), index(1))> (maxValue*0.5)){
            //     grid_data(index(0), index(1)) = maxValue;
            // }
            // else{
            //     grid_data(index(0), index(1)) = NAN;
            // }
        }
    }
    

    void Narrowpassagedetection::create_ray(double pos_x, double pos_y, double yaw_, grid_map::GridMap map){
        grid_map::Position position(pos_x,pos_y);
        ray_buffer.clear();
        test_buffer.clear();
        // if(outputmap.getIndex(grid_map::Position(pos_x,pos_y),robot)){
        //     outputmap.getPosition(robot,position);
        // }
        double angle = 0.0;



        for(angle=-60.0; angle<60.0;){
            // double k = std::tan(angle/180.00*M_PI+yaw);
            // double b = position[1]-k*position[0];
            // tan90 = false;
            // if(k>70){
            //     tan90=true;
            //     b = position[0];
            // }
            double angleRadians;
            if(backward){
                angleRadians = angle/180.00*M_PI + yaw_ - M_PI;
            }
            else{
                angleRadians = angle/180.00*M_PI + yaw_;
            }
            
            double x = 3.0 * std::cos(angleRadians);
            double y = 3.0 * std::sin(angleRadians);
            // std::cout<<"k:  "<<k<< "  b:   "<<b<<std::endl;
            ray_detection(x,y,angle,position,map);
            
            angle +=0.10;
        }

        // if(!ray_buffer.empty())
        // {
        //     std::ofstream outputfile3("/home/haolei/Documents/ray_detection.txt");

        // if (outputfile3.is_open()){
        //     for (const auto& value : ray_buffer)
        //     {
        //         outputfile3<<"angle:  " <<value.angle <<"  distance: "<<value.distance <<"   index: "<< value.index[0]<<"   "<<value.index[1]<< "    positon:  "<<value.position[0]<<"   "<<value.position[1]<<"\n"; 
        //     }
        //     outputfile3.close();
        // }

        // }


    }

    void Narrowpassagedetection::create_ray2(double pos_x, double pos_y, double yaw_,grid_map::GridMap map){
        grid_map::Position position(pos_x,pos_y);
        ray_buffer.clear();
        test_buffer.clear();
        // if(outputmap.getIndex(grid_map::Position(pos_x,pos_y),robot)){
        //     outputmap.getPosition(robot,position);
        // }
        double angle = 0.0;



        for(angle=-95.0; angle<-80.0;){
            // double k = std::tan(angle/180.00*M_PI+yaw);
            // double b = position[1]-k*position[0];
            // tan90 = false;
            // if(k>70){
            //     tan90=true;
            //     b = position[0];
            // }
            double angleRadians;
            if(backward){
                angleRadians = angle/180.00*M_PI + yaw_ - M_PI;
            }
            else{
            angleRadians = angle/180.00*M_PI+yaw_;
            }
            
            double x = 3.0 * std::cos(angleRadians);
            double y = 3.0 * std::sin(angleRadians);
            // std::cout<<"k:  "<<k<< "  b:   "<<b<<std::endl;
            ray_detection2(x,y,angle,position,map);
            
            angle +=0.01;
        }

        for(angle=80.0; angle<95.0;){
            // double k = std::tan(angle/180.00*M_PI+yaw);
            // double b = position[1]-k*position[0];
            // tan90 = false;
            // if(k>70){
            //     tan90=true;
            //     b = position[0];
            // }
            double angleRadians;
            if(backward){
                angleRadians = angle/180.00*M_PI + yaw_ - M_PI;
            }
            else{
                angleRadians = angle/180.00*M_PI+yaw_;
            }
            
            double x = 3.0 * std::cos(angleRadians);
            double y = 3.0 * std::sin(angleRadians);
            // std::cout<<"k:  "<<k<< "  b:   "<<b<<std::endl;
            ray_detection2(x,y,angle,position,map);
            
            angle +=0.01;
        }
        // for(angle=40.0; angle<100.0;){
        //     // double k = std::tan(angle/180.00*M_PI+yaw);
        //     // double b = position[1]-k*position[0];
        //     // tan90 = false;
        //     // if(k>70){
        //     //     tan90=true;
        //     //     b = position[0];
        //     // }
        //     double angleRadians;
        //     if(backward){
        //         angleRadians = angle/180.00*M_PI + yaw_ - M_PI;
        //     }
        //     else{
        //         angleRadians = angle/180.00*M_PI+yaw_;
        //     }
            
        //     double x = 3.0 * std::cos(angleRadians);
        //     double y = 3.0 * std::sin(angleRadians);
        //     // std::cout<<"k:  "<<k<< "  b:   "<<b<<std::endl;
        //     ray_detection(x,y,angle,position,map);
            
        //     angle +=0.1;
        // }

        // if(!ray_buffer.empty())
        // {
        //     std::ofstream outputfile3("/home/haolei/Documents/ray_detection.txt");

        // if (outputfile3.is_open()){
        //     for (const auto& value : ray_buffer)
        //     {
        //         outputfile3<<"angle:  " <<value.angle <<"  distance: "<<value.distance <<"   index: "<< value.index[0]<<"   "<<value.index[1]<< "    positon:  "<<value.position[0]<<"   "<<value.position[1]<<"\n"; 
        //     }
        //     outputfile3.close();
        // }

        // }


    }

    bool Narrowpassagedetection::compute_angle_diff(double a1,double a2){
        int r1,r2;
        if(a1<M_PI/2 &&a1>0 &&a1==0)
        {
            r1=1;
        }
        else if(a1>M_PI/2&& a1==M_PI/2 &&a1< M_PI)
        {
            r1=2;
        }
        else if(a1>(-M_PI)&&a1==(-M_PI)&&a1<(-M_PI/2)){
            r1=3;
        }
        else{
            r1=4;
        }

        if(a2<M_PI/2 &&a2>0 &&a2==0)
        {
            r2=1;
        }
        else if(a2>M_PI/2&& a2==M_PI/2 &&a2< M_PI)
        {
            r2=2;
        }
        else if(a2>(-M_PI)&&a2==(-M_PI)&&a2<(-M_PI/2)){
            r2=3;
        }
        else{
            r2=4;
        }

        return true;
    }
    void Narrowpassagedetection::ray_detection(double x, double y,double angle, grid_map::Position robot_position,grid_map::GridMap map){
        dis_buffer.clear();
        for(grid_map::GridMapIterator iterator(map); !iterator.isPastEnd(); ++iterator)
        {
            const grid_map::Index index(*iterator);
            const float& value = map.get("elevation")(index(0), index(1));   // elevation  
            if(std::isfinite(value)){
                grid_map::Position position;
                map.getPosition(index,position);
                grid_map::Position position1(position[0]-robot_position[0],position[1]-robot_position[1]);
                grid_map::Position position2(x,y);
                // double diff = isPointOnSegment(position1,position2,length);
                if(isPointOnSegment(position1,position2))
                {
                    double dis = calculateDistance(position,robot_position);
                    dis_buffer.push_back({dis, index,position});
                }

            }
        }

        if(!dis_buffer.empty())
        {
            
            std::sort(dis_buffer.begin(),dis_buffer.end(), Narrowpassagedetection::compareByDis);
            grid_map::Index index = dis_buffer[0].index;
            int index1 = index[0];
            int index2 = index[1];
            auto result = std::find_if(ray_buffer.begin(), ray_buffer.end(), 
                [index1, index2](const auto& element) {
                    return element.index[0]==index1 && element.index[1]==index2;
                }
            );
            if(result == ray_buffer.end())
            {
                ray_buffer.push_back({angle,dis_buffer[0].distance,dis_buffer[0].index,dis_buffer[0].position});
            }
            // if(ray_buffer.empty()){
            //     ray_buffer.push_back({angle,dis_buffer[0].distance,dis_buffer[0].index,dis_buffer[0].position});
            // }
            // else if(ray_buffer.back().index[0]!=index[0]){
            //     ray_buffer.push_back({angle,dis_buffer[0].distance,dis_buffer[0].index,dis_buffer[0].position});
            // }
        }
    }

    void Narrowpassagedetection::ray_detection2(double x, double y,double angle, grid_map::Position robot_position,grid_map::GridMap map){
        dis_buffer.clear();
        for(grid_map::GridMapIterator iterator(map); !iterator.isPastEnd(); ++iterator)
        {
            const grid_map::Index index(*iterator);
            const float& value = map.get("elevation")(index(0), index(1));
            if(std::isfinite(value)){
                grid_map::Position position;
                map.getPosition(index,position);
                grid_map::Position position1(position[0]-robot_position[0],position[1]-robot_position[1]);
                grid_map::Position position2(x,y);
                // double diff = isPointOnSegment(position1,position2,length);
                if(isPointOnSegment(position1,position2,0.999))
                {
                    double dis = calculateDistance(position,robot_position);
                    dis_buffer.push_back({dis, index,position});
                }

            }
        }

        if(!dis_buffer.empty())
        {
            
            std::sort(dis_buffer.begin(),dis_buffer.end(), Narrowpassagedetection::compareByDis);
            grid_map::Index index = dis_buffer[0].index;
            int index1 = index[0];
            int index2 = index[1];
            auto result = std::find_if(ray_buffer.begin(), ray_buffer.end(), 
                [index1, index2](const auto& element) {
                    return element.index[0]==index1 && element.index[1]==index2;
                }
            );
            if(result == ray_buffer.end())
            {
                ray_buffer.push_back({angle,dis_buffer[0].distance,dis_buffer[0].index,dis_buffer[0].position});
            }
            // if(ray_buffer.empty()){
            //     ray_buffer.push_back({angle,dis_buffer[0].distance,dis_buffer[0].index,dis_buffer[0].position});
            // }
            // else if(ray_buffer.back().index[0]!=index[0]){
            //     ray_buffer.push_back({angle,dis_buffer[0].distance,dis_buffer[0].index,dis_buffer[0].position});
            // }
        }
    }

    double Narrowpassagedetection::calculateDistance(const grid_map::Position& A, const grid_map::Position& B){
        return std::sqrt(std::pow(B[0] - A[0], 2) + std::pow(B[1] - A[1], 2));
    }

    bool Narrowpassagedetection::compareByDis(const dis_buffer_type& a, const dis_buffer_type& b) {
            return a.distance < b.distance;
        }
    bool Narrowpassagedetection::compareByWidth(const passage_width_buffer_type&a ,const passage_width_buffer_type&b){
        return a.wide < b.wide;
    }
    
    bool Narrowpassagedetection::compute_passage_width(grid_map::GridMap map,geometry_msgs::Pose &pos){
        std::vector <ray_buffer_type> buffer1;
        std::vector <ray_buffer_type> buffer2;
        width_buffer.clear();
        double width;
        bool detected = false;
        for(int i = 1; i<ray_buffer.size();i++){

            if((std::abs(ray_buffer[i-1].angle-ray_buffer[i].angle)>10.0)){
                buffer1.clear();
                buffer2.clear();
                for(int j = 0;j<i;j++)
                {
                    buffer1.push_back(ray_buffer[j]);
                }
                for(int z = i;z<ray_buffer.size();z++ ){
                    buffer2.push_back(ray_buffer[z]);
                }
                width_buffer.clear();
                for(int i=0;i<buffer1.size();i++)
                {
                    for(int j=0; j<buffer2.size();j++)
                    {
                        const double distance = calculateDistance(buffer1[i].position,buffer2[j].position);
                        width_buffer.push_back({distance,buffer1[i].index,buffer2[j].index, buffer1[i].position, buffer2[j].position});
                    }
                }

                std::sort(width_buffer.begin(),width_buffer.end(), Narrowpassagedetection::compareByWidth);
                width = width_buffer[0].wide;
                if(width>0.3500){
                    detected = true;
                    break;
                }                
            }
        }

        // if(!detected){
        //     width=0;
        //     ROS_INFO("CLASSIFICATION");
        //     buffer1.clear();
        //     buffer2.clear();
        //     classification(buffer1, buffer2, ray_buffer);

        //     width_buffer.clear();
        //     if(buffer1.size()==0||buffer2.size()==0){
        //         return;
        //     }
        //     for(int i=0;i<buffer1.size();i++)
        //     {
        //         for(int j=0; j<buffer2.size();j++)
        //         {
        //             const double distance = calculateDistance(buffer1[i].position,buffer2[j].position);
        //             width_buffer.push_back({distance,buffer1[i].index,buffer2[j].index, buffer1[i].position, buffer2[j].position});
        //         }
        //     }

        //     std::sort(width_buffer.begin(),width_buffer.end(), Narrowpassagedetection::compareByWidth);
        //     width = width_buffer[0].wide;



        // }
        
        // std::ofstream outputfile6("/home/haolei/Documents/width_buffer.txt");
        //     if (outputfile6.is_open()){
        //         for (const auto& value : width_buffer)
        //         {
        //             outputfile6<<"width::  "<<value.wide<<"  index1 : "<<value.index1[0]<<"  "<<value.index1[1] <<"   index2: "<< value.index2[0]<<"   "<<value.index2[1]<< "\n"; 
        //         }
        //         outputfile6.close();
        //         ROS_INFO("save tay success\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
        //     }




        cv::Sobel(gaussian_img, gradientX, CV_32F, 1, 0, 3);
        cv::Sobel(gaussian_img, gradientY, CV_32F, 0, 1, 3);
        cv::cartToPolar(gradientX, gradientY, gradient, direction, true);
        // std::cout<<"compute "<<"\n\n\n\n\n\n\n\n\n"<<std::endl;
        gradient.convertTo(gradient,CV_8UC1);




        for(int i=0;i<gradient.rows;i++)  
        {  
            for(int j=0;j<gradient.cols;j++)  
            {  
                //magnitude.at<Vec3b>(i,j)[0]=magnitude.at<Vec3b>(i,j)[0]/div*div+div/2;  
                // std::cout<<magnitude.at<uchar>(i,j)<<std::endl;
                if(gradient.at<uchar>(i,j)< uchar(255))
                {
                    gradient.at<uchar>(i,j)=uchar(0);
                }
                else{
                    gradient.at<uchar>(i,j) = uchar(255);
                }
            }  
        }
        convert_from_gradient();


    }

    void Narrowpassagedetection::convert_from_gradient(){
        for (grid_map::GridMapIterator iterator(elevationmap_);!iterator.isPastEnd(); ++iterator ){
            const grid_map::Index index(*iterator);
            const float& value = grid_data(index(0), index(1));
            const grid_map::Index imageIndex(iterator.getUnwrappedIndex());
            const float maxValue = elevationmap_.get("elevation").maxCoeffOfFinites();
            if(gradient.at<uchar>(imageIndex(0),imageIndex(1))== uchar(0)&&std::isfinite(value)&&value<0.7*maxValue){
                grid_data(index(0), index(1)) = NAN;
            }          
            // else if(grid_data(index(0), index(1))> (maxValue*0.5)){
            //     grid_data(index(0), index(1)) = maxValue;
            // }
            // else{
            //     grid_data(index(0), index(1)) = NAN;
            // }
        }
    }
    

    void Narrowpassagedetection::create_ray(double pos_x, double pos_y, double yaw_, grid_map::GridMap map){
        grid_map::Position position(pos_x,pos_y);
        ray_buffer.clear();
        test_buffer.clear();
        // if(outputmap.getIndex(grid_map::Position(pos_x,pos_y),robot)){
        //     outputmap.getPosition(robot,position);
        // }
        double angle = 0.0;



        for(angle=-60.0; angle<60.0;){
            // double k = std::tan(angle/180.00*M_PI+yaw);
            // double b = position[1]-k*position[0];
            // tan90 = false;
            // if(k>70){
            //     tan90=true;
            //     b = position[0];
            // }
            double angleRadians;
            if(backward){
                angleRadians = angle/180.00*M_PI + yaw_ - M_PI;
            }
            else{
                angleRadians = angle/180.00*M_PI + yaw_;
            }
            
            double x = 3.0 * std::cos(angleRadians);
            double y = 3.0 * std::sin(angleRadians);
            // std::cout<<"k:  "<<k<< "  b:   "<<b<<std::endl;
            ray_detection(x,y,angle,position,map);
            
            angle +=0.10;
        }

        // if(!ray_buffer.empty())
        // {
        //     std::ofstream outputfile3("/home/haolei/Documents/ray_detection.txt");

        // if (outputfile3.is_open()){
        //     for (const auto& value : ray_buffer)
        //     {
        //         outputfile3<<"angle:  " <<value.angle <<"  distance: "<<value.distance <<"   index: "<< value.index[0]<<"   "<<value.index[1]<< "    positon:  "<<value.position[0]<<"   "<<value.position[1]<<"\n"; 
        //     }
        //     outputfile3.close();
        // }

        // }


    }

    void Narrowpassagedetection::create_ray2(double pos_x, double pos_y, double yaw_,grid_map::GridMap map){
        grid_map::Position position(pos_x,pos_y);
        ray_buffer.clear();
        test_buffer.clear();
        // if(outputmap.getIndex(grid_map::Position(pos_x,pos_y),robot)){
        //     outputmap.getPosition(robot,position);
        // }
        double angle = 0.0;



        for(angle=-95.0; angle<-80.0;){
            // double k = std::tan(angle/180.00*M_PI+yaw);
            // double b = position[1]-k*position[0];
            // tan90 = false;
            // if(k>70){
            //     tan90=true;
            //     b = position[0];
            // }
            double angleRadians;
            if(backward){
                angleRadians = angle/180.00*M_PI + yaw_ - M_PI;
            }
            else{
            angleRadians = angle/180.00*M_PI+yaw_;
            }
            
            double x = 3.0 * std::cos(angleRadians);
            double y = 3.0 * std::sin(angleRadians);
            // std::cout<<"k:  "<<k<< "  b:   "<<b<<std::endl;
            ray_detection2(x,y,angle,position,map);
            
            angle +=0.01;
        }

        for(angle=80.0; angle<95.0;){
            // double k = std::tan(angle/180.00*M_PI+yaw);
            // double b = position[1]-k*position[0];
            // tan90 = false;
            // if(k>70){
            //     tan90=true;
            //     b = position[0];
            // }
            double angleRadians;
            if(backward){
                angleRadians = angle/180.00*M_PI + yaw_ - M_PI;
            }
            else{
                angleRadians = angle/180.00*M_PI+yaw_;
            }
            
            double x = 3.0 * std::cos(angleRadians);
            double y = 3.0 * std::sin(angleRadians);
            // std::cout<<"k:  "<<k<< "  b:   "<<b<<std::endl;
            ray_detection2(x,y,angle,position,map);
            
            angle +=0.01;
        }
        // for(angle=40.0; angle<100.0;){
        //     // double k = std::tan(angle/180.00*M_PI+yaw);
        //     // double b = position[1]-k*position[0];
        //     // tan90 = false;
        //     // if(k>70){
        //     //     tan90=true;
        //     //     b = position[0];
        //     // }
        //     double angleRadians;
        //     if(backward){
        //         angleRadians = angle/180.00*M_PI + yaw_ - M_PI;
        //     }
        //     else{
        //         angleRadians = angle/180.00*M_PI+yaw_;
        //     }
            
        //     double x = 3.0 * std::cos(angleRadians);
        //     double y = 3.0 * std::sin(angleRadians);
        //     // std::cout<<"k:  "<<k<< "  b:   "<<b<<std::endl;
        //     ray_detection(x,y,angle,position,map);
            
        //     angle +=0.1;
        // }

        // if(!ray_buffer.empty())
        // {
        //     std::ofstream outputfile3("/home/haolei/Documents/ray_detection.txt");

        // if (outputfile3.is_open()){
        //     for (const auto& value : ray_buffer)
        //     {
        //         outputfile3<<"angle:  " <<value.angle <<"  distance: "<<value.distance <<"   index: "<< value.index[0]<<"   "<<value.index[1]<< "    positon:  "<<value.position[0]<<"   "<<value.position[1]<<"\n"; 
        //     }
        //     outputfile3.close();
        // }

        // }


    }

    bool Narrowpassagedetection::compute_angle_diff(double a1,double a2){
        int r1,r2;
        if(a1<M_PI/2 &&a1>0 &&a1==0)
        {
            r1=1;
        }
        else if(a1>M_PI/2&& a1==M_PI/2 &&a1< M_PI)
        {
            r1=2;
        }
        else if(a1>(-M_PI)&&a1==(-M_PI)&&a1<(-M_PI/2)){
            r1=3;
        }
        else{
            r1=4;
        }

        if(a2<M_PI/2 &&a2>0 &&a2==0)
        {
            r2=1;
        }
        else if(a2>M_PI/2&& a2==M_PI/2 &&a2< M_PI)
        {
            r2=2;
        }
        else if(a2>(-M_PI)&&a2==(-M_PI)&&a2<(-M_PI/2)){
            r2=3;
        }
        else{
            r2=4;
        }

        return true;
    }
    void Narrowpassagedetection::ray_detection(double x, double y,double angle, grid_map::Position robot_position,grid_map::GridMap map){
        dis_buffer.clear();
        for(grid_map::GridMapIterator iterator(map); !iterator.isPastEnd(); ++iterator)
        {
            const grid_map::Index index(*iterator);
            const float& value = map.get("elevation")(index(0), index(1));   // elevation  
            if(std::isfinite(value)){
                grid_map::Position position;
                map.getPosition(index,position);
                grid_map::Position position1(position[0]-robot_position[0],position[1]-robot_position[1]);
                grid_map::Position position2(x,y);
                // double diff = isPointOnSegment(position1,position2,length);
                if(isPointOnSegment(position1,position2))
                {
                    double dis = calculateDistance(position,robot_position);
                    dis_buffer.push_back({dis, index,position});
                }

            }
        }

        if(!dis_buffer.empty())
        {
            
            std::sort(dis_buffer.begin(),dis_buffer.end(), Narrowpassagedetection::compareByDis);
            grid_map::Index index = dis_buffer[0].index;
            int index1 = index[0];
            int index2 = index[1];
            auto result = std::find_if(ray_buffer.begin(), ray_buffer.end(), 
                [index1, index2](const auto& element) {
                    return element.index[0]==index1 && element.index[1]==index2;
                }
            );
            if(result == ray_buffer.end())
            {
                ray_buffer.push_back({angle,dis_buffer[0].distance,dis_buffer[0].index,dis_buffer[0].position});
            }
            // if(ray_buffer.empty()){
            //     ray_buffer.push_back({angle,dis_buffer[0].distance,dis_buffer[0].index,dis_buffer[0].position});
            // }
            // else if(ray_buffer.back().index[0]!=index[0]){
            //     ray_buffer.push_back({angle,dis_buffer[0].distance,dis_buffer[0].index,dis_buffer[0].position});
            // }
        }
    }

    void Narrowpassagedetection::ray_detection2(double x, double y,double angle, grid_map::Position robot_position,grid_map::GridMap map){
        dis_buffer.clear();
        for(grid_map::GridMapIterator iterator(map); !iterator.isPastEnd(); ++iterator)
        {
            const grid_map::Index index(*iterator);
            const float& value = map.get("elevation")(index(0), index(1));
            if(std::isfinite(value)){
                grid_map::Position position;
                map.getPosition(index,position);
                grid_map::Position position1(position[0]-robot_position[0],position[1]-robot_position[1]);
                grid_map::Position position2(x,y);
                // double diff = isPointOnSegment(position1,position2,length);
                if(isPointOnSegment(position1,position2,0.999))
                {
                    double dis = calculateDistance(position,robot_position);
                    dis_buffer.push_back({dis, index,position});
                }

            }
        }

        if(!dis_buffer.empty())
        {
            
            std::sort(dis_buffer.begin(),dis_buffer.end(), Narrowpassagedetection::compareByDis);
            grid_map::Index index = dis_buffer[0].index;
            int index1 = index[0];
            int index2 = index[1];
            auto result = std::find_if(ray_buffer.begin(), ray_buffer.end(), 
                [index1, index2](const auto& element) {
                    return element.index[0]==index1 && element.index[1]==index2;
                }
            );
            if(result == ray_buffer.end())
            {
                ray_buffer.push_back({angle,dis_buffer[0].distance,dis_buffer[0].index,dis_buffer[0].position});
            }
            // if(ray_buffer.empty()){
            //     ray_buffer.push_back({angle,dis_buffer[0].distance,dis_buffer[0].index,dis_buffer[0].position});
            // }
            // else if(ray_buffer.back().index[0]!=index[0]){
            //     ray_buffer.push_back({angle,dis_buffer[0].distance,dis_buffer[0].index,dis_buffer[0].position});
            // }
        }
    }

    double Narrowpassagedetection::calculateDistance(const grid_map::Position& A, const grid_map::Position& B){
        return std::sqrt(std::pow(B[0] - A[0], 2) + std::pow(B[1] - A[1], 2));
    }

    bool Narrowpassagedetection::compareByDis(const dis_buffer_type& a, const dis_buffer_type& b) {
            return a.distance < b.distance;
        }
    bool Narrowpassagedetection::compareByWidth(const passage_width_buffer_type&a ,const passage_width_buffer_type&b){
        return a.wide < b.wide;
    }
    
    bool Narrowpassagedetection::compute_passage_width(grid_map::GridMap map,geometry_msgs::Pose &pos){
        std::vector <ray_buffer_type> buffer1;
        std::vector <ray_buffer_type> buffer2;
        width_buffer.clear();
        double width;
        bool detected = false;
        for(int i = 1; i<ray_buffer.size();i++){

            if((std::abs(ray_buffer[i-1].angle-ray_buffer[i].angle)>10.0)){
                buffer1.clear();
                buffer2.clear();
                for(int j = 0;j<i;j++)
                {
                    buffer1.push_back(ray_buffer[j]);
                }
                for(int z = i;z<ray_buffer.size();z++ ){
                    buffer2.push_back(ray_buffer[z]);
                }
                width_buffer.clear();
                for(int i=0;i<buffer1.size();i++)
                {
                    for(int j=0; j<buffer2.size();j++)
                    {
                        const double distance = calculateDistance(buffer1[i].position,buffer2[j].position);
                        width_buffer.push_back({distance,buffer1[i].index,buffer2[j].index, buffer1[i].position, buffer2[j].position});
                    }
                }

                std::sort(width_buffer.begin(),width_buffer.end(), Narrowpassagedetection::compareByWidth);
                width = width_buffer[0].wide;
                if(width>0.3500){
                    detected = true;
                    break;
                }                
            }
        }

        // if(!detected){
        //     width=0;
        //     ROS_INFO("CLASSIFICATION");
        //     buffer1.clear();
        //     buffer2.clear();
        //     classification(buffer1, buffer2, ray_buffer);

        //     width_buffer.clear();
        //     if(buffer1.size()==0||buffer2.size()==0){
        //         return;
        //     }
        //     for(int i=0;i<buffer1.size();i++)
        //     {
        //         for(int j=0; j<buffer2.size();j++)
        //         {
        //             const double distance = calculateDistance(buffer1[i].position,buffer2[j].position);
        //             width_buffer.push_back({distance,buffer1[i].index,buffer2[j].index, buffer1[i].position, buffer2[j].position});
        //         }
        //     }

        //     std::sort(width_buffer.begin(),width_buffer.end(), Narrowpassagedetection::compareByWidth);
        //     width = width_buffer[0].wide;



        // }
        
        // std::ofstream outputfile6("/home/haolei/Documents/width_buffer.txt");
        //     if (outputfile6.is_open()){
        //         for (const auto& value : width_buffer)
        //         {
        //             outputfile6<<"width::  "<<value.wide<<"  index1 : "<<value.index1[0]<<"  "<<value.index1[1] <<"   index2: "<< value.index2[0]<<"   "<<value.index2[1]<< "\n"; 
        //         }
        //         outputfile6.close();
        //         ROS_INFO("save tay success\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
        //     }


       
        
        // publish width
        std_msgs::String width_msg;
        width_msg.data= std::to_string(width);
        width_pub.publish(width_msg);

        if(width<0.650&& width>0.350){
            if(is_on_path(width_buffer, path_msg, pos))
            {
                return (is_obstacle(width_buffer[0],map));
            }
            
        }
        return false;
    }

    bool Narrowpassagedetection::is_obstacle(const passage_width_buffer_type& a,grid_map::GridMap map){
        const grid_map::Position position1 = a.position1;
        const grid_map::Position position2 = a.position2;
        int num_obstacle=0;
        for(grid_map::GridMapIterator iterator(map); !iterator.isPastEnd(); ++iterator)
        {
            
            const grid_map::Index index(*iterator);
            grid_map::Position position3;
            map.getPosition(index,position3);
            if(isPointOnSegment(position1,position2,position3)||isPointOnSegment(position2,position1,position3)){
                grid_map::Index occupancy_index;
                occupancy_map.getIndex(position3,occupancy_index);
                if(occupancy_map["occupancy"](occupancy_index[0],occupancy_index[1])==0 || occupancy_map["occupancy"](occupancy_index[0],occupancy_index[1])==NAN){
                    grid_map::Index index2;
                    outputmap.getIndex(position3,index2);
                    outputmap["elevation"](index2[0],index2[1]) = 0;
                    // num_no_obstacle++;
                }
                else{
                    num_obstacle++;
                }
            }
        }
        if(num_obstacle>5){
            return false;
        }
        return true;
        
    }

    void Narrowpassagedetection::mark_narrow_passage(const passage_width_buffer_type& a){
        const grid_map::Position position1 = a.position1;
        const grid_map::Position position2 = a.position2;

        for(grid_map::GridMapIterator iterator(outputmap); !iterator.isPastEnd(); ++iterator)
        {
            const grid_map::Index index(*iterator);
            grid_map::Position position3;
            outputmap.getPosition(index,position3);
            if(isPointOnSegment(position1,position2,position3)||isPointOnSegment(position2,position1,position3)){
                outputmap["elevation"](index[0],index[1]) = 0;
            }
        }


    }

    bool Narrowpassagedetection::isPointOnSegment(const grid_map::Position A, const grid_map::Position B, const grid_map::Position C)
    {
        double vectorAB_x = B[0] - A[0];
        double vectorAB_y = B[1] - A[1];
        double vectorAC_x = C[0] - A[0];
        double vectorAC_y = C[1] - A[1];

        double dotProduct = vectorAB_x * vectorAC_x + vectorAB_y * vectorAC_y;

        double lengthAB = calculateDistance(A, B);

        double lengthAC = calculateDistance(A, C);

        // 判断点 C 是否在线段 AB 上
        return ((dotProduct/(lengthAB*lengthAC) > 0.95)&& lengthAC<lengthAB && lengthAC!=0.0);
    }


    bool Narrowpassagedetection::isPointOnSegment(const grid_map::Position A, const grid_map::Position B, float max)
    {
        double vectorOA_x = A[0] - 0;
        double vectorOA_y = A[1] - 0;
        double vectorOB_x = B[0] - 0;
        double vectorOB_y = B[1] - 0;

        double dotProduct = vectorOA_x * vectorOB_x + vectorOA_y * vectorOB_y;

        grid_map::Position O(0,0);
        double lengthOB = calculateDistance(O, B);
        double lenghtOA = calculateDistance(O,A);
        // 判断点 C 是否在线段 AB 上
        return (dotProduct/(lenghtOA*lengthOB)>max);
    }

    void Narrowpassagedetection::classification(std::vector<ray_buffer_type> &buffer1, std::vector<ray_buffer_type> &buffer2, const std::vector<ray_buffer_type> &data_)
    {
        std::vector<ray_buffer_type> data_buffer = data_;
        std::sort(data_buffer.begin(),data_buffer.end(),Narrowpassagedetection::compareByPose);
        buffer1.push_back(data_buffer[0]);
        int size= data_buffer.size();
        for(int i=0; i<size;i++){
            for (int j=0; j<data_buffer.size();j++){
                if(std::abs(data_buffer[j].position[0]-buffer1.back().position[0])<0.25&&std::abs(data_buffer[j].position[1]-buffer1.back().position[1])<0.25){
                    buffer1.push_back(data_buffer[j]);
                    data_buffer.erase(data_buffer.begin()+j);
                    break;
                }
                if(j==(data_buffer.size()-1)){
                    buffer2 = data_buffer;
                }
            }

        }
    }


    bool Narrowpassagedetection::compareByPose(const ray_buffer_type &a, const ray_buffer_type &b){

        return (a.position[0] != b.position[0]) ? (a.position[0] < b.position[0]) : (a.position[1] < b.position[1]);
    
    }

    int Narrowpassagedetection::get_path_index(const nav_msgs::Path path_msg, const float distance ){
        int count = path_msg.poses.size();
        float distance_ = 0.0;
        int index = 1; 
        for(; index<count; index++){
            distance_ +=std::sqrt(std::pow(path_msg.poses[index].pose.position.x-path_msg.poses[index-1].pose.position.x,2)+ std::pow(path_msg.poses[index].pose.position.y - path_msg.poses[index-1].pose.position.y,2));
            if(distance_>distance){
                return index;
            }
        }
        return index;
    }

    geometry_msgs::Pose Narrowpassagedetection::extend_point(geometry_msgs::Pose pose, float distance){
        tf::Quaternion quat;
        tf::quaternionMsgToTF(pose.orientation, quat);
        double roll_, pitch_, yaw_;
        tf::Matrix3x3(quat).getRPY(roll_,pitch_,yaw_);
        double angle = yaw_;
        if(backward){
          angle -=M_PI;
        }
        double x = distance * std::cos(angle);
        double y = distance * std::sin(angle);

        geometry_msgs::Pose pose_;
        pose_.orientation = pose.orientation;
        pose_.position.x = pose.position.x + x;
        pose_.position.y = pose.position.y + y;
        pose_.position.z = pose.position.z;

        return pose_;

    }

    bool Narrowpassagedetection::finde_intersection_point (std::vector <passage_width_buffer_type> width_buffer, nav_msgs::Path& msg, geometry_msgs::Pose &pos){
        int count = msg.poses.size();
        // grid_map::Position mid_pos()
        float pos_x = (width_buffer[0].position1[0]+width_buffer[0].position2[0])/2.0;
        float pos_y = (width_buffer[0].position1[1]+width_buffer[0].position2[1])/2.0;
        int index = 0;
        float distance= MAXFLOAT;
        for(int i=0; i<count; i++){
            float cost = std::sqrt(std::pow(msg.poses[i].pose.position.x-pos_x,2) + std::pow(msg.poses[i].pose.position.y-pos_y, 2));
            if(cost<distance){
                distance = cost;
                index = i;
            }
        }
        if(distance<0.05){
            
            pos.position = msg.poses[index].pose.position;
//            pos.position.x = pos_x;
//            pos.position.y = pos_y;
            return true;
        }
        return false;
    }


     bool Narrowpassagedetection::is_on_path (std::vector <passage_width_buffer_type> width_buffer, nav_msgs::Path& msg, geometry_msgs::Pose &pos){
        int count = msg.poses.size();
        // grid_map::Position mid_pos()
        float pos_x = (width_buffer[0].position1[0]+width_buffer[0].position2[0])/2.0;
        float pos_y = (width_buffer[0].position1[1]+width_buffer[0].position2[1])/2.0;
        int index = 0;
        float distance= MAXFLOAT;
        for(int i=0; i<count; i++){
            float cost = std::sqrt(std::pow(msg.poses[i].pose.position.x-pos_x,2) + std::pow(msg.poses[i].pose.position.y-pos_y, 2));
            if(cost<distance){
                distance = cost;
                index = i;
            }
        }
        if(distance<0.05){
            
            pos = msg.poses[index].pose;
            // pos.position = msg.poses[index].pose.position;

            return true;
        }
        return false;
    }


    void Narrowpassagedetection::extend_point_publisher(geometry_msgs::Pose point){
        geometry_msgs::PoseStamped msg;
        msg.header.frame_id = "narrow_approach";
        msg.header.stamp = ros::Time::now();
        msg.header.seq = 1;
        msg.pose = point;
        extend_point_pub.publish(msg);
    }

    bool Narrowpassagedetection::approach_distance(nav_msgs::Odometry robot_pose_msg ,geometry_msgs::Pose mid_pose, float &distance, nav_msgs::Path path_msg){
        float x_1 = robot_pose_msg.pose.pose.position.x;
        float y_1 = robot_pose_msg.pose.pose.position.y;
        float x_2 = mid_pose.position.x;
        float y_2 = mid_pose.position.y;
        float distance_=0;
        int count = path_msg.poses.size();
        distance_ = std::sqrt(std::pow(x_1-x_2,2) + std::pow(x_2-y_2,2));
//        for (int i =1; i<count;i++){
//            distance_ +=std::sqrt(std::pow(path_msg.poses[i-1].pose.position.x-path_msg.poses[i].pose.position.x, 2) + std::pow(path_msg.poses[i-1].pose.position.y - path_msg.poses[i].pose.position.y, 2));
//            if(std::sqrt(std::pow(x_2-path_msg.poses[i].pose.position.x,2 ) + std::pow(y_2 - path_msg.poses[i].pose.position.y, 2))< 0.1){
//                break;
//            }
//
//        }
        distance = distance_;

        if(distance_<2.5){
            return true;
        }
        return false;
    }


}