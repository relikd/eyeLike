#include "findPupils.hpp"
#include "../Helper/Debug.h"
#include "ExCuSe/algo.h"

using namespace Detector;

static Debug debugEye(EyeImage);

Pupils::Pupils(const char* path) {
	if (path) {
#ifdef _WIN32
		fopen_s(&file, path, "w");
#else
		file = fopen(path, "w");
#endif
		if (file) fprintf(file, "pLx,pLy,pRx,pRy,PupilDistance,cLx,cLy,cRx,cRy,CornerDistance\n");
	}
}

inline void addIntOffset(cv::Point2f &base, cv::Point2i offset) {
	base.x += offset.x;
	base.y += offset.y;
}

void printDebugOutput(FILE* f, cv::Point2f a, cv::Point2f b, bool isCorner) {
	double dist = cv::norm(a - b);
	if (isCorner) printf(" | ");
	printf("%s: ([%1.1f,%1.1f],[%1.1f,%1.1f], dist: %1.1f)", (isCorner?"corner":"pupil"), a.x, a.y, b.x, b.y, dist);
	if (isCorner) printf("\n");
	if (f) fprintf(f, "%1.1f,%1.1f,%1.1f,%1.1f,%1.2f%c", a.x, a.y, b.x, b.y, dist, (isCorner?'\n':','));
}

PointPair Pupils::findCorners( cv::Mat faceROI, RectPair cornerRegion, cv::Point2i offset ) {
	// find eye corner
	cv::Point2f leftCorner  = detectCorner.findByAvgColor(faceROI(cornerRegion.first), true);
	cv::Point2f rightCorner = detectCorner.findByAvgColor(faceROI(cornerRegion.second), false);
	addIntOffset(leftCorner, offset + cornerRegion.first.tl());
	addIntOffset(rightCorner, offset + cornerRegion.second.tl());
	
	printDebugOutput(file, leftCorner, rightCorner, true);
	
	return std::make_pair(leftCorner, rightCorner);
}

cv::Point2f Pupils::findSingle( cv::Mat faceROI ) {
	cv::Rect2i rectWithoutEdge(faceROI.cols*0.1, faceROI.rows*0.1, faceROI.cols*0.8, faceROI.rows*0.8);
	return findPupil( faceROI, rectWithoutEdge, true );
}

PointPair Pupils::find( cv::Mat faceROI, RectPair eyes, cv::Point2i offset ) {
#if DEBUG_PLOT_ENABLED
	debugEye.setImage(faceROI);
#endif
	
	//-- Find Eye Centers
	cv::Point2f leftPupil = findPupil( faceROI, eyes.first, true );
	cv::Point2f rightPupil = findPupil( faceROI, eyes.second, false );
	addIntOffset(leftPupil, offset);
	addIntOffset(rightPupil, offset);

	printDebugOutput(file, leftPupil, rightPupil, false);
	
	//cv::Rect roi( cv::Point( 0, 0 ), faceROI.size());
	//cv::Mat destinationROI = debugFace( roi );
	//faceROI.copyTo( destinationROI );
	
#if DEBUG_PLOT_ENABLED
	debugEye.display(window_name_face);
#endif
	
	return std::make_pair(leftPupil, rightPupil);
}

cv::Point2f Pupils::findPupil( cv::Mat faceImage, cv::Rect2i eyeRegion, bool isLeftEye )
{
	if (eyeRegion.area()) {
#if USE_EXCUSE_EYETRACKING
		// ExCuSe eye tracking
		cv::Mat sub = faceImage(eyeRegion);
		cv::Mat pic_th = cv::Mat::zeros(sub.rows, sub.cols, CV_8U);
		cv::Mat th_edges = cv::Mat::zeros(sub.rows, sub.cols, CV_8U);
		cv::RotatedRect elipse = run(&sub, &pic_th, &th_edges, true);
		cv::Point2f pupil = elipse.center;
#else
		// Gradient based eye tracking (Timm)
		cv::Point2f pupil = detectCenter.findEyeCenter(faceImage, eyeRegion, (isLeftEye ? window_name_left_eye : window_name_right_eye) );
#endif
		
//		cv::Mat empty = cv::Mat::zeros(faceImage.rows, faceImage.cols, CV_8U);
//		addIntOffset(elipse.center, eyeRegion.tl());
//		ellipse(empty, elipse, 123);
//		imshow("tmp", empty);
		
		if (pupil.x < 0.5 && pupil.y < 0.5) {
			if (kUseKalmanFilter) {
				// Reuse last point if no pupil found (eg. eyelid closed)
				pupil = (isLeftEye ? KFL : KFR).previousPoint();
			} else {
				// reset any near 0,0 value to actual 0,0 to indicate a 'not found'
				pupil = cv::Point2f();
			}
		}
		
		if (kUseKalmanFilter) {
			pupil = (isLeftEye ? KFL : KFR).smoothedPosition( pupil );
		}
		
		
#if DEBUG_PLOT_ENABLED
		// get tiled eye region
		//  .-----------.
		//  |___________|
		//  |      |    |
		//  | L    *  R |  // * = pupil
		//  |______|____|
		//  |           |
		//  '-----------'
		cv::Rect2f leftRegion(eyeRegion.x, eyeRegion.y, pupil.x, eyeRegion.height / 2);
		leftRegion.y += leftRegion.height / 2;
		
		cv::Rect2f rightRegion(leftRegion);
		rightRegion.x += pupil.x;
		rightRegion.width = eyeRegion.width - pupil.x;
		
		if (kEnableEyeCorner) {
			cv::Point2f leftCorner = detectCorner.find(faceImage(leftRegion), isLeftEye, false);
			cv::Point2f rightCorner = detectCorner.find(faceImage(rightRegion), isLeftEye, true);
			debugEye.addCircle( leftCorner + leftRegion.tl() , 200 );
			debugEye.addCircle( rightCorner + rightRegion.tl() , 200 );
		}
		
		// draw eye region
		debugEye.addRectangle(eyeRegion);
		
		// draw tiled eye box
		debugEye.addRectangle(leftRegion, 200);
		debugEye.addRectangle(rightRegion, 200);
		
		// draw eye center
		addIntOffset(pupil, eyeRegion.tl());
		debugEye.addCircle(pupil);
#else
		addIntOffset(pupil, eyeRegion.tl());
#endif
		return pupil;
	}
	return cv::Point2f();
}