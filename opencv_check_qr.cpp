
//	DS QR

// Library of OpenCV
#include<opencv2/objdetect/objdetect.hpp>
#include<opencv2/highgui/highgui.hpp>
#include<opencv2/imgproc/imgproc.hpp>
#include<opencv2/core/core.hpp>
#include<opencv2/contrib/contrib.hpp>
//==============================================
// Library of C++
#include <stdio.h>
#include <stdlib.h>
#include <sstream>
#include <string>
#include <netdb.h>
#include <iostream>
#include <unistd.h>
#include <string.h>
//==============================================
// Library of Socket
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

using namespace cv;
using namespace std;

//==============================================
//Declare Function to process
string to_string(int i);
void receiveImage(int sock, int idimage);
void recognizeGender(Mat image, string hardKey);
void makePOST(string harwareKey, int id_gender);

//_______________Check again_____________________
int constrain_check(int pre);
static int pre1 = -1;
static int pre2 = -1;
//____________________________________________

//==============================================
//Declare global variables
static int idImage = 0;
static string face_cascade_name =
		"/usr/local/share/OpenCV/haarcascades/haarcascade_frontalface_alt.xml";
static string eyes_cascade_name =
		"/usr/local/share/OpenCV/haarcascades/haarcascade_mcs_nose.xml";
// Global variables for Socket
// Receive Image
static int sockfd1, newsockfd1;
//-------PORT NUMBER-------
//for Cloud DCN
//static int portno = 20222;
//for local network
static int portno = 22222;
// Send Result
static int sockfd2;
static struct sockaddr_in serv_addr2;
static struct hostent *server2;
// OpenCV QR - Port DS Server QR
static int portno2 = 20281;

//OpenCV NFC - Port DS Server NFC
//static int portno2 = 20282;

//String to create POST message
static string basic = "key=basic&HK=";
static string man = "key=man&HK=";
static string woman = "key=woman&HK=";
static string crowded = "key=crowded&HK=";

static string length;
static string space = "\r\n\r\n";
static string content, contentSend;
static int lengthContent;

static const char* request;

static string httpRequest = "POST /xibo-cms/listener.php HTTP/1.1\r\n"
		"Host: dcn306.asuscomm.com \r\n"
		"Content-Type: application/x-www-form-urlencoded\r\n"
		"Content-Length: ";

// Can move to Main functions
static Ptr<FaceRecognizer> model = createLBPHFaceRecognizer();
// Detect Face to recognize
static CascadeClassifier face_cascade;
static CascadeClassifier eyes_cascade;

//==============================================
// Main Function
int main() {

	// Preprocessing for recognize face
	model->load("database_gender.xml");
	face_cascade.load(face_cascade_name);
	eyes_cascade.load(eyes_cascade_name);

	//======================================

	//Socket to receive Image
	int pid;
	socklen_t clilen;
	struct sockaddr_in serv_addr, cli_addr;
	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(portno);
	sockfd1 = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd1 < 0)
		perror("ERROR opening socket");

	if (bind(sockfd1, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
		perror("ERROR on binding");

	//======================================
	//Socket to send result
	server2 = gethostbyname("dcn306.asuscomm.com");
	if (server2 == NULL) {
		fprintf(stderr, "ERROR, no such host\n");
		exit(0);
	}
	bzero((char *) &serv_addr2, sizeof(serv_addr2));
	serv_addr2.sin_family = AF_INET;
	bcopy((char *) server2->h_addr,
	(char *)&serv_addr2.sin_addr.s_addr,
	server2->h_length);
	serv_addr2.sin_port = htons(portno2);

	//======================================

	listen(sockfd1, 10);
	clilen = sizeof(cli_addr);
	for (;;) {
		idImage++;
		newsockfd1 = accept(sockfd1, (struct sockaddr *) &cli_addr, &clilen);

		if (newsockfd1 < 0) {
			perror("ERROR on accept");
			exit(1);
		}
		
		receiveImage(newsockfd1, idImage);
		close(newsockfd1);
		/*
		// Create child process
		pid = fork();

		if (pid < 0) {
			perror("ERROR on fork");
			exit(1);
		}

		if (pid == 0) {
			// This is the client process 
			close(sockfd1);
			receiveImage(newsockfd1, idImage);
			exit(0);
		} else {
			close(newsockfd1);
		}
		*/
	}
	return 0;
}

//==============================================

void receiveImage(int newsockfd1, int idImage) {

	//cout << "----------------------" << idImage << "---------------------- "<< endl;

	// Creat variables for processing
	string keyHardware;
	int ptr = 0;
	int bytes;
	char key[255];

	// Size image = (480,640)
	//Mat img = Mat::zeros(480, 640, CV_8UC3);
	// Size image = (720,1280)
	Mat img = Mat::zeros(720, 1280, CV_8UC3);

	int imgSize = img.total() * img.elemSize();
	uchar sockData[imgSize];
	for (int i = 0; i < imgSize; i += bytes) {
		if ((bytes = recv(newsockfd1, sockData + i, imgSize - i, 0)) == -1) {
			perror("ERROR on accept");
		}
	}

	// Assign pixel value to img
	for (int i = 0; i < img.rows; i++) {
		for (int j = 0; j < img.cols; j++) {
			img.at<cv::Vec3b>(i, j) = cv::Vec3b(sockData[ptr + 0],
					sockData[ptr + 1], sockData[ptr + 2]);
			ptr = ptr + 3;
		}
	}
	// Get Key Hardware
	bytes = recv(newsockfd1, key, 255, 0);
	key[bytes] = '\0';
	keyHardware = key;
	//cout << "ID Client = " << keyHardware << endl;
	//string pidImage = to_string(idImage) + ".jpg";
	//cout << "Image received: " << pidImage << endl;
	//imwrite(pidImage, img);
	//imwrite("output.png", img);
	recognizeGender(img, keyHardware);

	//cout << "-------------------END--------------------\n \n" << endl;
}

//==============================================

void recognizeGender(Mat image, string key) {
	cout << "-----------------" << idImage  << "----------------"  <<endl;
	int temp2;
	int predictedLabel;
	// Process Image
	if (!image.data) {
		cout << "Could not open or find the image" << endl;
	}

	Mat testSample, grayImage;
	cvtColor(image, grayImage, CV_BGR2GRAY);
	equalizeHist(grayImage, testSample);

	Mat face_resized;
	vector<Rect_<int> > faces;
	vector<Rect> eyes;

	face_cascade.detectMultiScale(testSample, faces, 1.1, 4,
	CV_HAAR_DO_CANNY_PRUNING | CV_HAAR_SCALE_IMAGE, Size(20, 20),Size(720, 720));

	if (faces.size() == 1) {

		// Process face by face
		Rect face_i = faces[0];
		Mat face = testSample(face_i);

		//imwrite("out.jpg", face);

		eyes_cascade.detectMultiScale(face, eyes);
		if (eyes.size() > 0) {
			//Mat face_resized;
			resize(face, face_resized, Size(200, 200), 1.0, 1.0, INTER_CUBIC);
			//imwrite("out.jpg", face_resized);
			predictedLabel = model->predict(face_resized);
			temp2 =constrain_check (predictedLabel);
			//Send POST message to DS Server
			makePOST(key, temp2);
			if (temp2 == 0)
				cout << "\n Gender = MAN" << endl;
			else
				cout << "\n Gender = WOMAN" << endl;
		} else {
			pre1 = -1;
			pre2 = -1;
			cout << "\n \n" << endl;
			//cout << "DO NOT HAVE ANYONE" << endl;
			makePOST(key, 4);
		}
	} else if (faces.size() > 1) {
		//cout << "\n Face size = " << faces.size() << endl;

		unsigned int i, n = 0;
		for (i = 0; i < faces.size(); i++) {
			// For the face processing
			Rect face_i = faces[i];
			Mat face = testSample(face_i);
			//imwrite("out.jpg", face);
	
			// To recognize eyes
			eyes_cascade.detectMultiScale(face, eyes);
			if (eyes.size() > 0) {
				n++;
				//cout << "Nose size  = " << n << endl;
				resize(face, face_resized, Size(200, 200), 1.0, 1.0,
						INTER_CUBIC);
			}
		}
		if (n > 1) {
			cout << "\n CROWDED" << endl;
			//Send POST message to DS Server
			pre1 = -1;
			pre2 = -1;
			makePOST(key, 2);
		} else if (n == 1) {

			//imwrite("out.jpg", face_resized);

			predictedLabel = model->predict(face_resized);
			temp2 =constrain_check (predictedLabel);
			//Send POST message to DS Server
			makePOST(key, temp2);
			if (temp2 == 0)
				cout << "\n Gender = MAN1" << endl;
			else
				cout << "\n Gender = WOMAN1" << endl;

		} else {
			pre1 = -1;
			pre2 = -1;
			cout << "\n \n" << endl;
			makePOST(key, 4);
		}
	} else {
		pre1 = -1;
		pre2 = -1;
		cout << "\n \n" << endl;
		makePOST(key, 4);
	}
	cout << "---------------END---------------" << endl;
}

//==============================================

void makePOST(string harwareKey, int id_gender) {
	
	// Creat data send to DS server
	switch (id_gender) {
	case 0:
		content = man + harwareKey;
		break;
	case 1:
		content = woman + harwareKey;
		break;
	case 2:
		content = crowded + harwareKey;
		break;
	default:
		content = basic + harwareKey;
	}
	
	lengthContent = int(content.length());
	length = to_string(lengthContent);
	contentSend = httpRequest + length + space + content;
	request = contentSend.c_str();
	
	//Creat socket to send result
	sockfd2 = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd2 < 0)
		perror("ERROR opening socket");
	if (connect(sockfd2, (struct sockaddr *) &serv_addr2, sizeof(serv_addr2))
			< 0)
		perror("ERROR connecting");
	send(sockfd2, request, strlen(request), 0);
	close(sockfd2);
}

//==============================================

string to_string(int i) {
	std::stringstream ss;
	ss << i;
	return ss.str();
}
//==============================================
int constrain_check( int pre){
	int temp = 0;
	//cout << "pre = "<< pre << endl;
	//if (pre == 0)
	//	cout << "pre = MAN"<< endl;
	//else
	//	cout << "pre = WOMAN"<< endl;
	
	
	//cout << "pre1 = " << pre1 << endl;
	//cout << "pre2 = " << pre2  << endl;
	if (pre2 == -1){
	//	cout << "Loop pre2 = -1" << endl;
		pre2 = pre1;
		pre1 = pre;
		temp = pre;
		}
	else {
		// pre 1 # -1
		if (pre == pre1){
	//		cout << "Loop pre1 = pre" << endl;
			pre2 =pre1;
			temp = pre;
			}
		else if (pre == pre2){
	//		cout << "Loop pre2 = pre" << endl;
			pre1 = pre;
			temp = pre;
			}
		else {
	//		cout << "Loop pre2 = pre1 # pre" << endl;
			pre1 = pre;
			temp = pre2;
			}
	}
	
	//if (temp == 0)
	//	cout << "pre = MAN"<< endl;
	//else
	//	cout << "pre = WOMAN"<< endl;
	return temp;
}
