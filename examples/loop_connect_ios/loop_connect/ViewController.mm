
#import "ViewController.h"

#include <memory>

#import <UIKit/UIButton.h>

#import "sdk/objc/base/RTCVideoRenderer.h"
#if defined(RTC_SUPPORTS_METAL)
#import "sdk/objc/components/renderer/metal/RTCMTLVideoView.h"  // nogncheck
#endif
#import "sdk/objc/components/renderer/opengl/RTCEAGLVideoView.h"
#import "sdk/objc/helpers/RTCCameraPreviewView.h"

#import "CallClientWithNoVideo.h"

@interface ViewController ()

@property(nonatomic) RTC_OBJC_TYPE(RTCCameraVideoCapturer) * capturer;
@property(nonatomic) __kindof UIView<RTC_OBJC_TYPE(RTCVideoRenderer)> *remoteVideoView;
@property(nonatomic) UIButton * button_init;
@property(nonatomic) UIButton * button;
@property(nonatomic) BOOL started;
@property(nonatomic) std::shared_ptr<CallClientWithNoVideo> call_client;

@end

@implementation ViewController {
  UIView *_view;
}

@synthesize capturer = _capturer;
@synthesize button_init = _button_init;
@synthesize button = _button;
@synthesize started = _started;
@synthesize call_client = _call_client;

- (void)loadView {
  _view = [[UIView alloc] initWithFrame:CGRectZero];
  
#if defined(RTC_SUPPORTS_METAL)
  _remoteVideoView = [[RTC_OBJC_TYPE(RTCMTLVideoView) alloc] initWithFrame:CGRectZero];
#else
  _remoteVideoView = [[RTC_OBJC_TYPE(RTCEAGLVideoView) alloc] initWithFrame:CGRectZero];
#endif
  _remoteVideoView.translatesAutoresizingMaskIntoConstraints = NO;
  [_view addSubview:_remoteVideoView];
  
  _button_init = [UIButton buttonWithType:UIButtonTypeSystem];
  _button_init.translatesAutoresizingMaskIntoConstraints = NO;
  [_button_init setTitle:@"Initialize" forState:UIControlStateNormal];
  [_button_init setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
  [_button_init addTarget:self action:@selector(loopCallInitialize) forControlEvents:UIControlEventTouchUpInside];
  [_view addSubview:_button_init];
  
  _button = [UIButton buttonWithType:UIButtonTypeSystem];
  _button.translatesAutoresizingMaskIntoConstraints = NO;
  [_button setTitle:@"Start loop call" forState:UIControlStateNormal];
  [_button setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
  [_button addTarget:self
              action:@selector(loopCallControl)
    forControlEvents:UIControlEventTouchUpInside];
  [_view addSubview:_button];
  
  UILayoutGuide *margin = _view.layoutMarginsGuide;
  [_remoteVideoView.leadingAnchor constraintEqualToAnchor:margin.leadingAnchor].active = YES;
  [_remoteVideoView.topAnchor constraintEqualToAnchor:margin.topAnchor].active = YES;
  [_remoteVideoView.trailingAnchor constraintEqualToAnchor:margin.trailingAnchor].active = YES;
  [_remoteVideoView.bottomAnchor constraintEqualToAnchor:margin.bottomAnchor].active = YES;
  
  [_button_init.leadingAnchor constraintEqualToAnchor:margin.leadingAnchor constant:8.0].active =
  YES;
  [_button_init.bottomAnchor constraintEqualToAnchor:margin.bottomAnchor constant:8.0].active = YES;
  [_button_init.widthAnchor constraintEqualToConstant:100].active = YES;
  [_button_init.heightAnchor constraintEqualToConstant:40].active = YES;
  
  [_button.trailingAnchor constraintEqualToAnchor:margin.trailingAnchor constant:8.0].active =
  YES;
  [_button.bottomAnchor constraintEqualToAnchor:margin.bottomAnchor constant:8.0].active =
  YES;
  [_button.widthAnchor constraintEqualToConstant:100].active = YES;
  [_button.heightAnchor constraintEqualToConstant:40].active = YES;
  
  self.view = _view;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  
  self.call_client = nullptr;
}

- (void)createAndInitializeVideoCaptur {
  self.capturer = [[RTC_OBJC_TYPE(RTCCameraVideoCapturer) alloc] init];
  // Start capturer.
  AVCaptureDevice *selectedDevice = nil;
  NSArray<AVCaptureDevice *> *captureDevices =
  [RTC_OBJC_TYPE(RTCCameraVideoCapturer) captureDevices];
  for (AVCaptureDevice *device in captureDevices) {
    if (device.position == AVCaptureDevicePositionFront) {
      selectedDevice = device;
      break;
    }
  }
  
  AVCaptureDeviceFormat *selectedFormat = nil;
  int targetWidth = 640;
  int targetHeight = 480;
  int currentDiff = INT_MAX;
  NSArray<AVCaptureDeviceFormat *> *formats =
  [RTC_OBJC_TYPE(RTCCameraVideoCapturer) supportedFormatsForDevice:selectedDevice];
  for (AVCaptureDeviceFormat *format in formats) {
    CMVideoDimensions dimension = CMVideoFormatDescriptionGetDimensions(format.formatDescription);
    FourCharCode pixelFormat = CMFormatDescriptionGetMediaSubType(format.formatDescription);
    int diff = abs(targetWidth - dimension.width) + abs(targetHeight - dimension.height);
    if (diff < currentDiff) {
      selectedFormat = format;
      currentDiff = diff;
    } else if (diff == currentDiff && pixelFormat == [_capturer preferredOutputPixelFormat]) {
      selectedFormat = format;
    }
  }
  
  [self.capturer startCaptureWithDevice:selectedDevice format:selectedFormat fps:30];
  NSLog(@"createAndInitializeVideoCaptur");
}

- (void)loopCallInitialize{
  if (self.call_client) {
    NSLog(@"Loop call client has been created");
  } else {
    self.call_client = std::make_shared<CallClientWithNoVideo>();
    [self createAndInitializeVideoCaptur];
  }
}

- (void)loopCallControl{
  if (!self.call_client) {
    NSLog(@"Loop call client has not been created");
    return;
  }
  if (self.started) {
    self.call_client->StopLoopCall();
    [self.button setTitle:@"Start loop call" forState:UIControlStateNormal];
  } else {
    self.call_client->StartLoopCall();
    [self.button setTitle:@"Stop loop call" forState:UIControlStateNormal];
  }
  self.started = !self.started;
}

@end
