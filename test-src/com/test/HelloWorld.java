package com.test;

public class HelloWorld {
    public static void main(String[] args) {
	System.out.println("Hello World!\n");
	String s = new String("This is cool");
	System.out.println(s.length());
	String name = System.getProperty("user.name");
	System.out.println(name);
    }
}
