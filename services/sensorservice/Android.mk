LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	BatteryService.cpp \
	CorrectedGyroSensor.cpp \
	FirewallConfig.proto \
	Fusion.cpp \
	GravitySensor.cpp \
	LinearAccelerationSensor.cpp \
	OrientationSensor.cpp \
	PrivacyRules.cpp \
	RotationVectorSensor.cpp \
	SensorDevice.cpp \
	SensorFusion.cpp \
	SensorInterface.cpp \
	SensorPerturb.cpp \
	SensorService.cpp \

LOCAL_CFLAGS:= -DLOG_TAG=\"SensorService\"

LOCAL_SHARED_LIBRARIES := \
	libcutils \
	libhardware \
	libutils \
	libbinder \
	libui \
	libgui \
	libstlport \

LOCAL_STATIC_LIBRARIES := \
	libprotobuf-cpp-2.3.0-full \

LOCAL_MODULE := libsensorservice

LOCAL_C_INCLUDES := \
	bionic \
	bionic/libstdc++/include \
	external/stlport/stlport \

include $(BUILD_SHARED_LIBRARY)
