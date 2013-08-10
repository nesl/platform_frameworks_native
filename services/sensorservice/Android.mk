LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	BatteryService.cpp \
	CorrectedGyroSensor.cpp \
	FirewallConfigMessages.proto \
	FirewallConfigUtils-inl.h \
	Fusion.cpp \
	GravitySensor.cpp \
	LinearAccelerationSensor.cpp \
	OrientationSensor.cpp \
	RotationVectorSensor.cpp \
	SensorDevice.cpp \
	SensorFusion.cpp \
	SensorInterface.cpp \
	SensorCountMessages.proto \
	SensorPerturb.cpp \
	PrivacyRules.cpp \
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
	libprotobuf-cpp-2.3.0-lite \

LOCAL_MODULE := libsensorservice

LOCAL_C_INCLUDES := \
	bionic \
	bionic/libstdc++/include \
	external/stlport/stlport \

include $(BUILD_SHARED_LIBRARY)

# Build a host executable for creating firewall configs.
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	FirewallConfigMessages.proto \
	FirewallConfigUtils-inl.h \
	FirewallConfig.cpp \

LOCAL_CFLAGS:= -DLOG_TAG=\"SensorFirewall-Config\"

LOCAL_MODULE:= sensorfirewall-config

LOCAL_SHARED_LIBRARIES := \
	libcutils \
	libstlport \

LOCAL_STATIC_LIBRARIES := \
	libprotobuf-cpp-2.3.0-lite

LOCAL_C_INCLUDES := \
	bionic \
	bionic/libstdc++/include \
	external/stlport/stlport \

include $(BUILD_EXECUTABLE)
