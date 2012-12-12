package com.test;

public class HelloWorld {
    private void method4() {
	System.out.println("inside method4()");
	this.method3();
    }

    public void method1() {
	System.out.println("inside method1()");
	this.method3();
	return;
    }

    public void method2() {
	System.out.println("inside method2()");
	this.method4();
	return;
    }

    public void method3() {
	System.out.println("inside method3()");
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
