include("D:/CloneCpp/HostApp/build/Desktop_Qt_6_10_1_MinGW_64_bit-Debug/.qt/QtDeploySupport.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/HostApp-plugins.cmake" OPTIONAL)
set(__QT_DEPLOY_I18N_CATALOGS "qtbase")

qt6_deploy_runtime_dependencies(
    EXECUTABLE "D:/CloneCpp/HostApp/build/Desktop_Qt_6_10_1_MinGW_64_bit-Debug/HostApp.exe"
    GENERATE_QT_CONF
)
