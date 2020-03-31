package com.jsdroid.groovy;

import android.util.Log;

import org.codehaus.groovy.ast.ClassNode;
import org.codehaus.groovy.control.CompilationFailedException;
import org.codehaus.groovy.control.CompilationUnit;
import org.codehaus.groovy.control.CompilerConfiguration;
import org.codehaus.groovy.control.SourceUnit;

import java.security.AccessController;
import java.security.PrivilegedAction;

import groovyjarjarasm.asm.ClassWriter;

/**
 * 通过此类编译groovy代码
 */
final class AndroidGroovyClassLoader extends groovy.lang.GroovyClassLoader {
    DexBytecodeProcessor dexBytecodeProcessor;

    private ClassLoader parent;

    public AndroidGroovyClassLoader(ClassLoader loader, CompilerConfiguration config, DexBytecodeProcessor dexBytecodeProcessor) {
        super(loader, config);
        this.parent = loader;
        this.dexBytecodeProcessor = dexBytecodeProcessor;
    }

    /**
     * 部分手机此方法不仅仅抛出ClassNotFoundException,导致加载groovy文件失败
     *
     * @param name
     * @return
     * @throws ClassNotFoundException
     */
    @Override
    public Class<?> loadClass(String name) throws ClassNotFoundException {
        try {
            Class clazz = super.loadClass(name);
            if (clazz != null) {
                return clazz;
            }
        } catch (Throwable e) {
            Log.e("JsDroid", "loadClass: ", e);
        }
        return parentLoadClass(name);
    }

    private Class parentLoadClass(String name) throws ClassNotFoundException {
        try {
            Class clazz = parent.loadClass(name);
            if (clazz != null) {
                return clazz;
            }
        } catch (Throwable e) {
            Log.e("JsDroid", "loadClass: ", e);
        }
        Log.e("JsDroid", "loadClass: ", new Exception());
        throw new ClassNotFoundException();
    }

    /**
     * 部分手机此方法不仅仅抛出ClassNotFoundException,导致加载groovy文件失败
     *
     * @param name
     * @param lookupScriptFiles
     * @param preferClassOverScript
     * @param resolve
     * @return
     * @throws ClassNotFoundException
     * @throws CompilationFailedException
     */
    @Override
    public Class loadClass(String name, boolean lookupScriptFiles, boolean preferClassOverScript, boolean resolve) throws ClassNotFoundException, CompilationFailedException {
        try {
            return super.loadClass(name, lookupScriptFiles, preferClassOverScript, resolve);
        } catch (Throwable e) {
            return parentLoadClass(name);
        }
    }

    /**
     * 部分手机此方法不仅仅抛出ClassNotFoundException,导致加载groovy文件失败
     *
     * @param name
     * @param lookupScriptFiles
     * @param preferClassOverScript
     * @return
     * @throws ClassNotFoundException
     * @throws CompilationFailedException
     */
    @Override
    public Class loadClass(String name, boolean lookupScriptFiles, boolean preferClassOverScript) throws ClassNotFoundException, CompilationFailedException {
        try {
            return super.loadClass(name, lookupScriptFiles, preferClassOverScript);
        } catch (Throwable e) {
            return parentLoadClass(name);
        }
    }

    /**
     * 部分手机此方法不仅仅抛出ClassNotFoundException,导致加载groovy文件失败
     *
     * @param name
     * @param resolve
     * @return
     * @throws ClassNotFoundException
     */
    @Override
    protected Class loadClass(String name, boolean resolve) throws ClassNotFoundException {
        try {
            return super.loadClass(name, resolve);
        } catch (Throwable e) {
            return parentLoadClass(name);
        }
    }

    /**
     * 此方法用于捕捉类生成事件
     *
     * @param unit
     * @param su
     * @return
     */
    @Override
    protected ClassCollector createCollector(CompilationUnit unit, SourceUnit su) {
        InnerLoader loader = AccessController.doPrivileged(new PrivilegedAction<InnerLoader>() {
            public InnerLoader run() {
                return new InnerLoader(AndroidGroovyClassLoader.this);
            }
        });
        return new ClassCollector(loader, unit, su) {
            @Override
            protected Class onClassNode(ClassWriter classWriter, ClassNode classNode) {
                try {
                    return super.onClassNode(classWriter, classNode);
                } catch (Exception e) {
                }
                return null;
            }

            @Override
            protected Class createClass(byte[] code, ClassNode classNode) {
                dexBytecodeProcessor.processBytecode(classNode, code);
                return super.createClass(code, classNode);
            }
        };
    }

}
