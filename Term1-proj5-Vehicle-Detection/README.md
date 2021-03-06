## Writeup

**Vehicle Detection Project**

The goals / steps of this project are the following:

* Perform a Histogram of Oriented Gradients (HOG) feature extraction on a labeled training set of images and train a classifier Linear SVM classifier
* Optionally apply a color transform and append binned color features, as well as histograms of color, to the HOG feature vector. 
* Note: for those first two steps don't forget to normalize the features and randomize a selection for training and testing.
* Implement a sliding-window technique and use the trained classifier to search for vehicles in images.
* Run the pipeline on a video stream (start with the test_video.mp4 and later implement on full project_video.mp4) and create a heat map of recurring detections frame by frame to reject outliers and follow detected vehicles.
* Estimate a bounding box for vehicles detected.

[//]: # (Image References)


[image5]: ./examples/bboxes_and_heat.png
[image6]: ./examples/labels_map.png
[image7]: ./examples/output_bboxes.png
[video1]: ./project_video.mp4

<p align="center">
 <a href=""><img src="./videos/out_video.gif" alt="Overview" width="50%" height="50%"></a>
</p>

## [Rubric](https://review.udacity.com/#!/rubrics/513/view) Points
Here I will consider the rubric points individually and describe how I addressed each point in my implementation.  

---
### Writeup / README

#### 1. Provide a Writeup / README that includes all the rubric points and how you addressed each one.

You're reading it!

### Histogram of Oriented Gradients (HOG)

#### 1. Explain how (and identify where in your code) you extracted HOG features from the training images.

HOG feature extraction is implemented in the function `extract_hog_features()` in `feature_extracters.py`). This function transforms the input image to the YCrCb colorspace and eventually calls `skimage.feature.hog()` to extract the HOG features. 

Features were extracted from images of vehicle and non-vehicle classes. One example of each class is shown below.

[imageCar]: ./output_images/car.png
![alt text][imageCar]

[imageNotCar]: ./output_images/non-car.png
![alt text][imageNotCar]

#### 2. Explain how you settled on your final choice of HOG parameters.

I explored different parameter settings for HOG feature extraction and converged to the parameter set below. This set had the best test-accuracy of 0.98 for a linear SVM classifier, and seemed to aid car detection best with few false positives. 

``` python
# HOG feature params
'use_gray_img':False,
'hog_channel':'ALL',
'hog_cspace':'YCrCb',
'hog_n_orientations': 9,
'hog_pixels_per_cell': 8,
'hog_cells_per_block': 2,
```

Here is an example of feature extraction using the above parameter values:

[image2]: ./output_images/HOG_example.png
![alt text][image2]

#### 3. Describe how (and identify where in your code) you trained a classifier using your selected HOG features (and color features if you used them).

In addition to the HOG features which capture object shape, I used color features to capture object appearance. The color features consisted of a downsampled image and histograms of intensity values in the individual channels of the image. I used the following parameter set for extracting these features:

```python
'color_cspace':'YCrCb',
'color_spatial_size':(32, 32),
'color_hist_bins':32,
'color_hist_range':(0, 256)
```

I found that using color features in addition to HOG features further improved the test accuracy of the linear SVM classifier while reducing the false positive rate further.

For each training (car / non-car) image, the HOG- and color-features are concatenated to form a one single feature vector. These feature vectors are stacked to form the matrix `X`, and labels car ("1") or non-car ("0") corresponding to each feature vector  are stored in a vector of `label`'s (see function `X, labels = get_training_data()` in `classifiers.py`). 

Then, a linear SVM classifier is fitted to the training data in the function `fit_svm(X, labels)` in file `classifiers.py`. Before training the classifier, the columns of the stacked feature vectors are normalized using `sklearn.preprocessing.StandardScaler()` in order to avoid any particular feature dominating the others by sheer scale. 

The training data is split into training (80 %) and test data (20 %) and randomy shuffled. An accuracy score for the classification is calculated on the test data using `svc.score()`.

### Sliding Window Search

#### 1. Describe how (and identify where in your code) you implemented a sliding window search.  How did you decide what scales to search and how much to overlap windows?

Initially I implemented a mechanism where a list of windows (x, y-coordinates) was generated for a region of interest in an image, given window size and overlap (`slide_window()` function in `sliding_windows.py`).  This windows list was then filtered for windows containing cars using the function `search_windows()` in `sliding_windows.py`. The `search_windows()` function extracted HOG and color features for each window and classified the window as either containing a car or not. This process was computationally very expensive, since the HOG features were extracted separately for each window. 

To reduce this computationaly complexity, I altered the above mechanism so that the HOG features are extracted only once for a region of interest in an image. Later during window classification, only the portion of the large HOG feature array inside that window is considered. This refined mechanism is implemented in the function `find_cars()` in file `sliding_windows.py`.

The image below shows an example of the sliding windows over which the classifier runs:

[image3]: ./output_images/all_windows_multiscale.png
![alt text][image3]

#### 2. Show some examples of test images to demonstrate how your pipeline is working. What did you do to optimize the performance of your classifier?

The `__name__ == '__main__'` function in `main_vehicle_detection.py` trains the linear SVM classifier and then calls `main_test_images()` which searches for cars in the image using the `find_cars()` function from `sliding_window.py`. The function `main_test_images()` also plots the images at various stages in the pipeline.

Here are some example images:

[image4]: ./output_images/hotwindows_cars_heatmap.png
![alt text][image4]

[imagePipeline2]: ./output_images/hotwindows_cars_heatmap2.png
![alt text][imagePipeline2]

---

### Video Implementation

#### 1. Provide a link to your final video output.  Your pipeline should perform reasonably well on the entire project video (somewhat wobbly or unstable bounding boxes are ok as long as you are identifying the vehicles most of the time with minimal false positives.)

Here's a [link to my video result](./out_video.mp4)


#### 2. Describe how (and identify where in your code) you implemented some kind of filter for false positives and some method for combining overlapping bounding boxes.

The `find_cars()` function in `sliding_windows.py` returns a list of "hot" windows -- windows which are classified as containing a car. By itself, this list contains some misclassified windows, so-called false positives. 

Typically, these are single random instances of misclassification, which we should disappear from one frame to the next, if the feature extraction and image classification performs well. Based on this hypothesis, I accumulated postitive detections over a certain number of consecutive frames into a heatmap. If an area in the image is consistently detected over consecutive frames, it will accumulate a higher heat value. On the other hand, single random instances of misclassified detections will not accumulate as much heat. Thresholding the heatmap should then remove these false positives. These ideas are implemented in the `get_heat_based_bboxes()`, `add_heat()`, and `apply_heat_threshold()` functions in `sliding_windows.py`.

I then used `scipy.ndimage.measurements.label()` to identify individual blobs in the heatmap.  I assumed each blob corresponded to a vehicle and drew tight bounding boxes around the blobs to indicate the vehicle. 

---

### Discussion

#### 1. Briefly discuss any problems / issues you faced in your implementation of this project.  Where will your pipeline likely fail?  What could you do to make it more robust?

**Feature normalization**
The training images provided were in PNG format. When read using `matplotlib.image.imread()`, their intensity values are in the range [0, 1]. The feature scaler `sklearn.preprocessing.StandardScaler` is fit to this data.

On the other hand, the test-images and the frames from the video have intensity ranges of [0. 255]. Failure to correctly normalize the test-images as `image = image.astype(np.float32)/255` leads to wrong transformation through the `StandardScaler` during feature extraction. This in turn leads to a lot of false positives and is very confounding.

**Colorspace**
While testing with the test-images, it seemed to me that performance was good for feature extraction in the `HSV` colorspace. So I stuck to it also for video processing, only to find out at a later stage that in fact the `YCrCb` space was much more suitable. This cost me quite some time. 

**False positives**
The current pipeline, although performing quite well, still gives occasional false positives. These are sometimes off-road (e.g. trees, fence) or on the other side of the road. Prior knowledge of where we are on the road and how wide the road is could be used to filter them out.

**A deep-learning approach?**
Also, the parameter space for feature extraction, classification, and sliding-window search is vast. The parameter values I have determined my overfit the project video and not perform quite so well on other data. I am curious to try a deep-learning approach like LeNet that chooses its own features/parameters and is known to perform well for image classification problems. 




-- Original Udacity text below --

✂ ― ✂ ― ✂ ― ✂ ― ✂ ― ✂ ― ✂ ― ✂ ― ✂ ― ✂ ― ✂ ― ✂ ― ✂ ― ✂ ― ✂ ― ✂ ― ✂ ― ✂ ― 

# Vehicle Detection
[![Udacity - Self-Driving Car NanoDegree](https://s3.amazonaws.com/udacity-sdc/github/shield-carnd.svg)](http://www.udacity.com/drive)


In this project, your goal is to write a software pipeline to detect vehicles in a video (start with the test_video.mp4 and later implement on full project_video.mp4), but the main output or product we want you to create is a detailed writeup of the project.  Check out the [writeup template](https://github.com/udacity/CarND-Vehicle-Detection/blob/master/writeup_template.md) for this project and use it as a starting point for creating your own writeup.  

Creating a great writeup:
---
A great writeup should include the rubric points as well as your description of how you addressed each point.  You should include a detailed description of the code used in each step (with line-number references and code snippets where necessary), and links to other supporting documents or external references.  You should include images in your writeup to demonstrate how your code works with examples.  

All that said, please be concise!  We're not looking for you to write a book here, just a brief description of how you passed each rubric point, and references to the relevant code :). 

You can submit your writeup in markdown or use another method and submit a pdf instead.

The Project
---

The goals / steps of this project are the following:

* Perform a Histogram of Oriented Gradients (HOG) feature extraction on a labeled training set of images and train a classifier Linear SVM classifier
* Optionally, you can also apply a color transform and append binned color features, as well as histograms of color, to your HOG feature vector. 
* Note: for those first two steps don't forget to normalize your features and randomize a selection for training and testing.
* Implement a sliding-window technique and use your trained classifier to search for vehicles in images.
* Run your pipeline on a video stream (start with the test_video.mp4 and later implement on full project_video.mp4) and create a heat map of recurring detections frame by frame to reject outliers and follow detected vehicles.
* Estimate a bounding box for vehicles detected.

Here are links to the labeled data for [vehicle](https://s3.amazonaws.com/udacity-sdc/Vehicle_Tracking/vehicles.zip) and [non-vehicle](https://s3.amazonaws.com/udacity-sdc/Vehicle_Tracking/non-vehicles.zip) examples to train your classifier.  These example images come from a combination of the [GTI vehicle image database](http://www.gti.ssr.upm.es/data/Vehicle_database.html), the [KITTI vision benchmark suite](http://www.cvlibs.net/datasets/kitti/), and examples extracted from the project video itself.   You are welcome and encouraged to take advantage of the recently released [Udacity labeled dataset](https://github.com/udacity/self-driving-car/tree/master/annotations) to augment your training data.  

Some example images for testing your pipeline on single frames are located in the `test_images` folder.  To help the reviewer examine your work, please save examples of the output from each stage of your pipeline in the folder called `ouput_images`, and include them in your writeup for the project by describing what each image shows.    The video called `project_video.mp4` is the video your pipeline should work well on.  

**As an optional challenge** Once you have a working pipeline for vehicle detection, add in your lane-finding algorithm from the last project to do simultaneous lane-finding and vehicle detection!

**If you're feeling ambitious** (also totally optional though), don't stop there!  We encourage you to go out and take video of your own, and show us how you would implement this project on a new video!

## How to write a README
A well written README file can enhance your project and portfolio.  Develop your abilities to create professional README files by completing [this free course](https://www.udacity.com/course/writing-readmes--ud777).

