package com.jsdroid.api;

public class JsDroidEnv {
    public static String shellServerFile = System.getenv("server_apk");
    public static String sdkFile = System.getenv("sdk_apk");
    public static String optDir = "/data/local/tmp/jsd_opt";
    public static String libDir = "/data/local/tmp/jsd_lib";
    public static String classesDir = "/data/local/tmp/jsd_classes";
    public static String pluginDir = "/data/local/tmp/jsd_plugin";
    public static String serverClass = "com.jsdroid.server.JsDroidServer";


}
