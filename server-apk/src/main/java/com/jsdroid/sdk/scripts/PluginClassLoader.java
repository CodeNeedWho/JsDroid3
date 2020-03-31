package com.jsdroid.sdk.scripts;

import com.jsdroid.api.JsDroidEnv;

import org.apache.commons.io.FileUtils;

import java.io.File;
import java.io.IOException;
import java.util.HashMap;
import java.util.Map;

import dalvik.system.DexClassLoader;

public class PluginClassLoader extends ClassLoader {

    static class PluginDexClassLoader extends BaseClassLoader {
        public PluginDexClassLoader(String dexPath, String optimizedDirectory, String librarySearchPath, ClassLoader parent) {
            super(dexPath, optimizedDirectory, librarySearchPath, parent);
        }

        @Override
        public Class<?> findClass(String name) throws ClassNotFoundException {
            return super.findClass(name);
        }
    }

    private static class Single {
        private static PluginClassLoader single = new PluginClassLoader();
    }

    public static PluginClassLoader getInstance() {
        return Single.single;
    }

    private PluginClassLoader() {
        super(PluginClassLoader.class.getClassLoader());

    }

    private Map<String, PluginDexClassLoader> classLoaderMap = new HashMap<>();

    public void add(String file) throws IOException {

        PluginDexClassLoader dexClassLoader = new PluginDexClassLoader(file,
                JsDroidEnv.optDir, JsDroidEnv.libDir, PluginClassLoader.class.getClassLoader());
        classLoaderMap.put(file, dexClassLoader);
    }

    @Override
    public Class<?> loadClass(String name) throws ClassNotFoundException {
        try {
            Class<?> aClass = super.loadClass(name);
            if (aClass != null) {
                return aClass;
            }
        } catch (Throwable e) {
            e.printStackTrace();
        }
        for (ClassLoader value : classLoaderMap.values()) {
            try {
                Class<?> aClass = value.loadClass(name);
                if (aClass != null) {
                    return aClass;
                }
            } catch (Throwable e) {
            }
        }
        return Thread.currentThread().getContextClassLoader().loadClass(name);
    }

    @Override
    protected Class<?> findClass(String name) throws ClassNotFoundException {
        try {
            Class<?> aClass = super.findClass(name);
            if (aClass != null) {
                return aClass;
            }
        } catch (Throwable e) {
            e.printStackTrace();
        }
        for (PluginDexClassLoader value : classLoaderMap.values()) {
            try {
                Class<?> aClass = value.findClass(name);
                if (aClass != null) {
                    return aClass;
                }
            } catch (Throwable e) {
            }
        }
        throw new ClassNotFoundException();
    }
}
