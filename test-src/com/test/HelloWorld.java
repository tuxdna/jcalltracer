package com.test;

public class HelloWorld {
    public void method1() {
	return;
    }
    public void method2() {
	return;
    }
    public void method3() {
	return;
    }
    public static void main(String[] args) {
	System.out.println("Hello World!\n");
	String s = new String("This is cool");
	System.out.println(s.length());
	String name = System.getProperty("user.name");
	System.out.println(name);

	HelloWorld hw = new HelloWorld();
	hw.method1();
	hw.method2();
	hw.method3();
	return;
    }
}
