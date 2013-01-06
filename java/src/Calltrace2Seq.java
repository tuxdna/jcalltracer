/*
 * Copyright 2009 Syed Ali Jafar Naqvi
 *           2013 Saleem Ansari <tuxdna@gmail.com>
 * 
 * This file is part of Java Call Tracer.
 *
 * Java Call Tracer is free software: you can redistribute it and/or modify
 * it under the terms of the Lesser GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Java Call Tracer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * Lesser GNU General Public License for more details.
 *
 * You should have received a copy of the Lesser GNU General Public License
 * along with Java Call Tracer.  If not, see <http://www.gnu.org/licenses/>.
 */

import java.awt.Font;
import java.awt.Graphics2D;
import java.awt.RenderingHints;
import java.awt.image.BufferedImage;
import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.FileWriter;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.OutputStreamWriter;
import java.io.PrintStream;
import java.io.StringReader;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import javax.imageio.ImageIO;

import org.apache.batik.transcoder.TranscoderException;
import org.apache.batik.transcoder.TranscoderInput;
import org.apache.batik.transcoder.TranscoderOutput;
import org.apache.batik.transcoder.image.JPEGTranscoder;
import org.apache.commons.cli.CommandLine;
import org.apache.commons.cli.CommandLineParser;
import org.apache.commons.cli.HelpFormatter;
import org.apache.commons.cli.Options;
import org.apache.commons.cli.ParseException;
import org.apache.commons.cli.PosixParser;
import org.apache.commons.lang.StringUtils;
import org.apache.xerces.parsers.DOMParser;
import org.w3c.dom.Document;
import org.w3c.dom.Element;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;
import org.w3c.dom.Text;
import org.xml.sax.InputSource;
import org.xml.sax.SAXException;

public class Calltrace2Seq {
	private static int nodeSep = 200;
	private static int nodeSep1 = 50;
	static int maxCallDepth = -1;
	static int minCallDepth = -1;
	static int totalDepth = 0;
	static int pos = 0;
	static int lastCnt = 0;
	static int lastTxtLen = 0;
	static String[] filters = new String[] { "L?([a-z0-9A-Z$]+)/",
			"\\((([a-z0-9A-Z$]+\\.)*[a-z0-9A-Z$]+)",
			"\\(\\[(([a-z0-9A-Z$]+\\.)*[a-z0-9A-Z$]+)",
			";(([a-z0-9A-Z$]+\\.)*[a-z0-9A-Z$]+)",
			";\\[(([a-z0-9A-Z$]+\\.)*[a-z0-9A-Z$]+)", ";", "\\)",
			":\\[(([a-z0-9A-Z$]+\\.)*[a-z0-9A-Z$]+)\\]" };
	static String[] replace = new String[] { "$1\\.", "\\($1", "\\($1\\[\\]",
			", $1", ", $1\\[\\]", "", "\\) :", ":$1\\[\\]\\]" };
	static boolean back = false;
	static boolean fwd = false;
	static List<Integer> lastPos = new ArrayList<Integer>();

    private static PrintStream printStream;
    static String packageTrimPattern = "([a-z0-9A-Z$]+\\.)+";
    static String inputXMLFile = "./calltrace.xml";
    static String outputFolder = "./";
    static String outputFileName = "calltrace";
    static int y = 0;
    static String svg = "";
    static HashMap<String, String> translate = new HashMap<String, String>();
    static {
        translate.put("I", "int");
        translate.put("V", "void");
        translate.put("D", "double");
        translate.put("E", "enum");
        translate.put("F", "float");
        translate.put("S", "short");
        translate.put("B", "byte");
        translate.put("Z", "boolean");
        translate.put("C", "char");
        translate.put("J", "long");
    }

    static List<String> classes = new ArrayList<String>();

    
	/**
	 * @param paths
	 * @return Return a combined path of the parameters provided in a portable
	 *         way.
	 */
	public static String joinPath(String... paths) {
		File file = new File(paths[0]);

		for (int i = 1; i < paths.length; i++) {
			file = new File(file, paths[i]);
		}

		return file.getPath();
	}


	private static void setup(String[] args) {
		// parse commandline parameters
		Options opts = new Options();
		opts.addOption("h", "help", false, "Display this help");
		opts.addOption("t", "trim-package", false, "Trim package part of a qualified class");
		opts.addOption("p", "trim-pattern", true,  "Package trim pattern");
		opts.addOption("d", "max-call-depth", true,  "Max Call Depth");
		opts.addOption("n", "min-call-depth", true,  "Min Call Depth");
		opts.addOption("i", "input-xml-file", true, "Input XML File");
		opts.addOption("o", "output-folder", true, "Output Folder");
		opts.addOption("f", "output-file", true, "Output File Name");

		CommandLineParser parser = new PosixParser();
		CommandLine cmd = null;
		try {
			cmd = parser.parse(opts, args);
		} catch (ParseException e) {
			e.printStackTrace();
			System.exit(1);
		}
		if (cmd.hasOption("h")) {
			// automatically generate the help statement
			HelpFormatter formatter = new HelpFormatter();
			formatter.printHelp(Calltrace2Seq.class.getName(), opts);
			System.exit(0);
		}
		
		boolean trimPackage = false;

		if (cmd.hasOption("d")) {
			trimPackage = true;
		}
		if (cmd.hasOption("p")) {
			packageTrimPattern = cmd.getOptionValue("p");
		}
		if (cmd.hasOption("d")) {
			maxCallDepth = Integer.parseInt(cmd.getOptionValue("d")) + 3;
		}
		if (cmd.hasOption("n")) {
			minCallDepth = Integer.parseInt(cmd.getOptionValue("n")) + 3;
		}
		if (cmd.hasOption("i")) {
			inputXMLFile = cmd.getOptionValue("i");
		}
		if (cmd.hasOption("o")) {
			outputFolder = cmd.getOptionValue("o");
		}
		if (cmd.hasOption("f")) {
			outputFileName = cmd.getOptionValue("f");
		}
		if (!trimPackage) {
			packageTrimPattern = "";
		}
	}

    private static void removeClintChild(NodeList nodes) {
        for (int i = 0; i < nodes.getLength(); i++) {
            int cnt = 0;
            Node node = nodes.item(i);
            removeClintChild(node.getChildNodes());
            if (node.getNodeName().equalsIgnoreCase("call")) {
                NodeList childNodes = node.getChildNodes();
                String className = "";
                for (int j = 0; j < childNodes.getLength(); j++) {
                    Node childNode = childNodes.item(j);
                    if (childNode.getNodeName().equalsIgnoreCase("class")) {
                        className = childNode.getTextContent().trim();
                    }
                    if (childNode.getNodeName().equalsIgnoreCase("method")) {
                        if (childNode.getTextContent().contains("<clinit> () :void")) {
                            Node parentNode = node.getParentNode();
                            parentNode.removeChild(node);
                            parentNode.normalize();
                            cnt++;
                            i--;
                            break;
                        } else if(childNode.getTextContent().contains("<init>")) {
                            ((Text)childNode.getFirstChild()).setNodeValue(className + childNode.getTextContent().trim().substring(6));
                        }
                    }
                }
            }
        }
    }

    private static void removeDuplicateChild(NodeList nodes) {
        for (int i = 0; i < nodes.getLength(); i++) {
            int cnt = 0;
            Node node = nodes.item(i);
            if (node.getNodeName().equalsIgnoreCase("call")) {
                for (int j = i + 1; j < nodes.getLength(); j++) {
                    Node sibling = nodes.item(j);
                    if (sibling.getNodeName().equalsIgnoreCase("call")) {
                        if (sibling.getTextContent().equals(node.getTextContent())) {
                            System.out.println("found duplicate removing it, " + i + ", " + j);
                            node.getParentNode().removeChild(sibling);
                            node.getParentNode().normalize();
                            cnt ++;
                            j --;
                        } else {
                            break;
                        }
                    }
                }
                ((Element)node).setAttribute("loop", "" + (cnt + 1));
            }
            removeDuplicateChild(node.getChildNodes());
        }
    }

    public static void main(String[] args) {

  //      try {
    	System.out.println("Calltrace2Seq\n");
        setup(args);
        DOMParser parser = new DOMParser();
        String file = inputXMLFile;
        try {
			parser.parse(new InputSource(new StringReader(applyRegExp(new File(file), filters, replace))));
		} catch (SAXException e2) {
			e2.printStackTrace();
			System.exit(1);
		} catch (IOException e2) {
			e2.printStackTrace();
			System.exit(1);
		}

        Document doc = parser.getDocument();
        removeDuplicateChild(doc.getChildNodes().item(0).getChildNodes());
        removeClintChild(doc.getChildNodes().item(0).getChildNodes());
        totalDepth = traverse(doc.getChildNodes(), 0);
        traverse(doc.getChildNodes(), 1, 0);
        svg = "<?xml version='1.0'?>\n" +
		"<svg onclick='remove(evt)' onload='init(evt)' " +
		     "xmlns='http://www.w3.org/2000/svg' height='" + ((((2 * (totalDepth - 1)) + 1) * 20) + 75) + 
		     "' width='" + ((classes.size() * nodeSep) + 200) + "'>\n" + "<defs>\n" + 
	        "<style type='text/css'>\n" + "rect, line, path { stroke-width: 2; stroke: black }\n" + 
		       "text { font-weight: bold }\n" + "<marker orient='auto' refY='2.5' refX='4' markerHeight='5'\n" +
	           "markerWidth='4' markerUnits='strokeWidth' id='mArrow'>\n" + "</marker>\n" + "</style>\n" +
	        "<script type='text/javascript'><![CDATA[" +
		        "var TrueCoords = null;" +
		        "var SVGDocument = null;" +
		        "var SVGRoot = null;" +
		        "var ClickPoint = null;\n" +
		        "function init(evt) {" +
		            "if ( window.svgDocument == null )" +
		                "window.svgDocument = evt.target.ownerDocument;"+
		            "SVGDocument=window.svgDocument;" +
		            "SVGRoot=SVGDocument.documentElement;" +
		            "TrueCoords=SVGRoot.createSVGPoint();" +
		            "ClickPoint=SVGRoot.createSVGPoint();" +
		        "}\n"+
		        "function remove(evt) {" +
		            "if(evt.target.nodeName != 'rect') {" +
		                "if(evt.target.nodeName == 'text' && evt.target.id == 'classTxt') {" +
		                "} else {" +
		                    "ou=window.svgDocument.getElementById('affiche');" +
		                    "if(ou.firstChild != null) {" +
		                        "ou.removeChild(ou.firstChild);" +
		                    "}" +
		                    "texte=window.svgDocument.createTextNode('');" +
		                    "ou.appendChild(texte);" +
		                "}" +
		            "}" +
		        "}\n" +
		        "function msOver(evt) {" +
		            "var transMatrix = evt.target.getCTM();" +
		            "GetTrueCoords(evt);" +
		            "if(typeof( window.pageYOffset ) == 'number') {" +
		                "ClickPoint.x= TrueCoords.x - Number(transMatrix.e) + window.pageXOffset;" +
		                "ClickPoint.y= TrueCoords.y - Number(transMatrix.f) + window.pageYOffset;" +
		            "}else if( SVGRoot.scrollLeft || SVGRoot.scrollTop ){" +
		                "ClickPoint.x= TrueCoords.x - Number(transMatrix.e) + SVGRoot.scrollTop;" +
		                "ClickPoint.y= TrueCoords.y - Number(transMatrix.f) + SVGRoot.scrollLeft;" +
		            "} else {" +
		                "ClickPoint.x= TrueCoords.x - Number(transMatrix.e);" +
		                "ClickPoint.y= TrueCoords.y - Number(transMatrix.f);" +
		            "}" +
		        "}\n" +
		        "function msClick(id, className, methodName) {" +
		            "root = window.svgDocument.getElementById(id);" +
		            "if(id == 'classTxt' || id == 'classRect') {" +
		                "ClickPoint.y = 20;" +
		            "}" +
		            "texte='';" +
		            "if(methodName != '') {" +
		                "texte=window.svgDocument.createTextNode(className + ' [' + methodName + ']');" +
		            "} else {" +
		                "texte=window.svgDocument.createTextNode(className);" +
		            "}" +
		            "ou=window.svgDocument.getElementById('affiche');" +
		            "ou.setAttribute('x',ClickPoint.x);ou.setAttribute('y',ClickPoint.y);" +
		            "ou.setAttribute('style','text-anchor:middle;font-size:10;font-family:Arial;fill:red');" +
		            "ou.setAttribute('font-size','10');" +
		            "if(ou.firstChild != null) {" +
		                "ou.removeChild(ou.firstChild);" +
		            "}" +
		            "ou.appendChild(texte);" +
		        "}\n" +
		        "function GetTrueCoords(evt) {" +
		            "var newScale = SVGRoot.currentScale;" +
		            "var translation = SVGRoot.currentTranslate;" +
		            "TrueCoords.x = (evt.clientX - translation.x)/newScale;" +
		            "TrueCoords.y = (evt.clientY - translation.y)/newScale;" +
		         "}\n" +
	        "]]></script>\n" +
	        "</defs>\n" + "<text font-family='Arial' font-variant='small-caps' font-weight='bold' " +
	        		"font-size='11' x='5' y='15'>\n" + "Thread - " + 
	        		doc.getFirstChild().getAttributes().getNamedItem("id").getFirstChild().getNodeValue() + 
	        		"\n" + "</text>\n" + svg + "<text id='affiche'></text>\n</svg>\n";

        
        File outputDirectory = new File(outputFolder);
        if(! outputDirectory.exists()) {
        	outputDirectory.mkdirs();
        }

		String svgFilePath = joinPath(outputFolder, outputFileName + ".svg");
		File svgFile = new File(svgFilePath);
		FileWriter fileWriter;
		try {
			fileWriter = new FileWriter(svgFile);
			fileWriter.write(svg);
			fileWriter.flush();
			fileWriter.close();
			System.out.println("Generated the SVG file: " + svgFile.getAbsolutePath());
		} catch (IOException e1) {
			System.out.println("SVG output failed: " + svgFile.getAbsolutePath());
			e1.printStackTrace();
		}
        
        try {			
			String traceFilePath = joinPath(outputFolder
					, outputFileName + ".trc");
			File traceFile = new File(traceFilePath);
			printStream = new PrintStream(traceFile);
			for (int i = 0; i < classes.size(); i++) {
				String className = classes.get(i);
				className = className.substring(className.lastIndexOf('.') + 1);
				if (className.length() > 33) {
					className = className.substring(0, 29) + "...";
				}
				printStream.print(StringUtils.rightPad(className, nodeSep1));
			}
            printStream.print("\n");
            traverse(doc.getChildNodes(), 2, 0);
            printStream.print("\n");
            printStream.close();
            
            String result = applyRegExp(traceFile, new String[]{"\\s+\n"}, new String[]{"\n"});

            File fout = traceFile;
            FileOutputStream fos = new FileOutputStream(fout);
            BufferedWriter out = new BufferedWriter(new OutputStreamWriter(fos));
            out.write(result);
            out.close();
            System.out.println("Generated the TRC file: " + traceFile.getAbsolutePath());

        } catch (IOException ex) {
			System.out.println("TRC output failed: " + svgFile);
            ex.printStackTrace();
        }

        
		String jpegFilePath = joinPath(outputFolder, outputFileName + ".jpg");
		File jpegFile = new File(jpegFilePath);
		try {
			JPEGTranscoder transcoder = new JPEGTranscoder();
			transcoder.addTranscodingHint(
					JPEGTranscoder.KEY_XML_PARSER_CLASSNAME,
					"org.apache.crimson.parser.XMLReaderImpl");
			transcoder.addTranscodingHint(JPEGTranscoder.KEY_QUALITY,
					new Float(1.0));
			TranscoderInput input = new TranscoderInput(new StringReader(svg));
			OutputStream ostream = new FileOutputStream(jpegFilePath);
			TranscoderOutput output = new TranscoderOutput(ostream);
			transcoder.transcode(input, output);
			ostream.close();
			System.out.println("Generated the JPG file: "
					+ jpegFile.getAbsolutePath());
		} catch (OutOfMemoryError ex) {
			BufferedImage test = new BufferedImage(670, 50,
					BufferedImage.TYPE_INT_RGB);
			Graphics2D g2 = test.createGraphics();
			g2.setRenderingHint(RenderingHints.KEY_ANTIALIASING,
					RenderingHints.VALUE_ANTIALIAS_ON);
			Font font = new Font("Courier", Font.BOLD, 10);
			g2.setFont(font);
			g2.drawString(
					"The image was not generated because the JVM heap size is too low. "
							+ "Increase the heap size and try again.", 30, 30);
			try {
				ImageIO.write(test, "jpg", jpegFile);
			} catch (IOException e) {
				e.printStackTrace();
			}
			System.out.println("Generated the JPG file ( using 2D ): "
					+ jpegFile.getAbsolutePath());
		} catch (TranscoderException e) {
			System.out.println("JPEG output failed"
					+ jpegFile.getAbsolutePath());
			e.printStackTrace();
		} catch (FileNotFoundException e) {
			System.out.println("JPEG output failed"
					+ jpegFile.getAbsolutePath());
			e.printStackTrace();
		} catch (IOException e) {
			System.out.println("JPEG output failed"
					+ jpegFile.getAbsolutePath());
			e.printStackTrace();
		}
    }

	public static String applyRegExp(File fin, String[] pattern,
			String[] replaceWith) {
		String result = "";
		try {
			FileInputStream fis = new FileInputStream(fin);

			BufferedReader in = new BufferedReader(new InputStreamReader(fis));
			String aLine = null;
			while ((aLine = in.readLine()) != null) {
				result += aLine + "\n";
			}
			in.close();
			result = applyRegExp(result, pattern, replaceWith);
		} catch (Exception e) {
		}
		return result;
	}

	public static String applyRegExp(String input, String[] pattern,
			String[] replaceWith) {
		String result = input;
		try {
			Object[] keys = translate.keySet().toArray();
			for (int j = 0; j < 10; j++) {
				for (int i = 0; i < keys.length; i++) {
					result = result.replaceAll("([;\\[\\(\\)])" + keys[i]
							+ "([IVDFSBZCJ]*[\\)L\\[\\]])",
							"$1" + translate.get(keys[i]) + ";$2");
				}
			}
			for (int i = 0; i < pattern.length; i++) {
				Pattern p = Pattern.compile(pattern[i]);
				Matcher m = p.matcher("");
				m.reset(result);
				result = m.replaceAll(replaceWith[i]);
			}
		} catch (Exception e) {
		}
		return result;
	}

	private static void traverse(NodeList nodes, int type, int cnt) {
		for (int i = 0; i < nodes.getLength(); i++) {
			Node node = nodes.item(i);
			if (node instanceof Text) {
				switch (type) {
				case 1:
					parse(node, cnt);
					break;
				case 2:
					process(node, cnt);
				}
			} else {
				traverse(node.getChildNodes(), type, cnt + 1);
			}
		}
	}

	// Method that generates the TRC file
	private static boolean process(Node node, int cnt) {
		if (node.getParentNode().getNodeName().equalsIgnoreCase("class")) {
			if (classes.indexOf(((Text) node).getNodeValue()) == 0 && cnt == 3) {
				printStream.print("\n\n");
				printStream.print(StringUtils.leftPad("", classes.size()
						* nodeSep1, "*"));
				printStream.print("\n\n");
			}
			if ((maxCallDepth == -1 || maxCallDepth >= cnt)
					&& (minCallDepth == -1 || minCallDepth <= cnt)) {
				if (lastCnt >= cnt) {
					printStream.print("  [return]");
					printStream.print("\n");
					int offset = 0;
					if (!lastPos.isEmpty() && cnt - 4 >= 0) {
						offset = lastPos.get(cnt - 4);
					}
					printStream.print(StringUtils
							.leftPad("", offset * nodeSep1));
					for (int i = 0; i < (classes.indexOf(((Text) node)
							.getNodeValue()) - offset - 1)
							* nodeSep1; i++) {
						printStream.print("-");
					}
					for (int i = 0; classes.indexOf(((Text) node)
							.getNodeValue()) - offset > 0
							&& i < nodeSep1 / 2; i++) {
						printStream.print("-");
					}
					printStream.print("(" + (cnt - 1) + ">");
					for (int i = 0; classes.indexOf(((Text) node)
							.getNodeValue()) - offset > 0
							&& i < (nodeSep1 / 2) - 4; i++) {
						printStream.print("-");
					}
					printStream.print(">");
				} else if (pos >= classes.indexOf(((Text) node).getNodeValue())) {
					printStream.print("\n");
					printStream
							.print(StringUtils.leftPad("",
									(classes.indexOf(((Text) node)
											.getNodeValue()) * nodeSep1)));
					if (pos > classes.indexOf(((Text) node).getNodeValue()))
						back = true;
				} else if (classes.indexOf(((Text) node).getNodeValue()) - pos >= 1) {
					fwd = true;
				}
			}
			pos = classes.indexOf(((Text) node).getNodeValue());
			lastCnt = cnt;
			if (lastPos.size() > cnt - 3) {
				lastPos.remove(cnt - 3);
			}
			if (lastPos.size() > cnt - 3) {
				lastPos.add(cnt - 3, pos);
			} else {
				lastPos.add(pos);
			}
		} else if (node.getParentNode().getNodeName()
				.equalsIgnoreCase("method")) {
			if ((maxCallDepth == -1 || maxCallDepth >= cnt)
					&& (minCallDepth == -1 || minCallDepth <= cnt)) {
				String method = ((Text) node).getNodeValue().replaceAll(
						packageTrimPattern, "");
				int len = method.length() < 40 ? method.length() : 40;
				String txt = cnt + "-" + method.substring(0, len);
				int offset = 0;
				if (!lastPos.isEmpty() && cnt - 4 >= 0) {
					offset = lastPos.get(cnt - 4);
				}
				if (fwd) {
					if (lastPos.size() > (cnt - 3)) {
						printStream.print(StringUtils.leftPad("",
								((lastPos.get(cnt - 3) - offset) * nodeSep1)
										- lastTxtLen - 1, "-"));
					}
					printStream.print(">");
				}
				printStream.print(txt);
				lastTxtLen = txt.length();
				if (back) {
					printStream.print("<");
					for (int i = 0; i < nodeSep1 - txt.length(); i++) {
						printStream.print("-");
					}
					printStream.print("<" + (cnt - 1) + ")");
					for (int i = 0; i < ((offset - pos - 1) * nodeSep1)
							+ (nodeSep1 / 4); i++) {
						printStream.print("-");
					}
					printStream.print("\n");
					printStream.print(StringUtils.leftPad("",
							((pos) * nodeSep1)));
					for (int i = 0; i < (nodeSep1 / 2) - 2; i++) {
						printStream.print("-");
					}
					printStream.print("(" + (cnt) + ">");
					for (int i = 0; i < (nodeSep1 / 2) - 2; i++) {
						printStream.print("-");
					}
					printStream.print(">");
				}
			}
			back = false;
			fwd = false;
			return true;
		}
		return false;
	}

	private static int traverse(NodeList nodes, int depth) {
		for (int i = 0; i < nodes.getLength(); i++) {
			Node node = nodes.item(i);
			if (node instanceof Text
					&& node.getParentNode().getNodeName()
							.equalsIgnoreCase("class")) {
				depth++;
			}
			depth = traverse(node.getChildNodes(), depth);
		}
		return depth;
	}

	private static int traverse(Node node, String className, int depth) {
		if (node == null) {
			return depth;
		}
		Node temp = null;
		try {
			temp = node.getFirstChild().getNextSibling().getFirstChild();
		} catch (Exception e) {
		}
		if (temp instanceof Text
				&& ((Text) temp).getNodeValue().equalsIgnoreCase(className)) {
			depth++;
		}
		depth = traverse(node.getParentNode(), className, depth);
		return depth;
	}

    //Method that generates SVG output
    private static void parse(Node node, int cnt) {
        if (node.getParentNode().getNodeName().equalsIgnoreCase("class")) {
            if (!classes.contains(((Text) node).getNodeValue())) {
                String className = ((Text) node).getNodeValue();
                className = className.substring(className.lastIndexOf('.') + 1);
                if (className.length() > 33) {
                    className = className.substring(0, 29) + "...";
                }
                int pre = (33 - className.length()) / 2;
                if (pre < 0) {
                    pre = 0;
                }
                svg += "<text id='classTxt' onmouseover='msOver(evt)' onclick=\"msClick('classTxt', '" + ((Text) node).getNodeValue() + "', '')\" y='40' font-family='Arial' font-variant='small-caps' font-weight='bold' font-size='10' x='" + ((classes.size() * nodeSep) + 30 + (pre * 5)) + "'>" + className + "</text>\n";
                svg += "<rect id='classRect' onmouseover='msOver(evt)' onclick=\"msClick('classRect', '" + ((Text) node).getNodeValue() + "', '')\" style='fill: none' height='20' width='195' y='25' x='" + ((classes.size() * nodeSep) + 22) + "' />\n";
                classes.add(((Text) node).getNodeValue());
                svg += "<line style='stroke-dasharray: 4,4; ' fill='none' stroke='black' x1='" + ((classes.indexOf(((Text) node).getNodeValue()) * nodeSep) + 126) + "' y1='55' x2='" + ((classes.indexOf(((Text) node).getNodeValue()) * nodeSep) + 126) + "' y2='" + ((((2 * (totalDepth - 1)) + 1) * 20) + 75) + "' />\n";
            }
            int loop = 0;
            if(node.getParentNode().getParentNode().hasAttributes()) {
                loop = Integer.parseInt(((Element)node.getParentNode().getParentNode()).getAttribute("loop"));
            }
            int depth = traverse(node.getParentNode().getParentNode().getChildNodes(), 0);

            Node temp = node.getParentNode().getParentNode().getParentNode().getFirstChild().getNextSibling().getFirstChild();
            int x = traverse(node.getParentNode().getParentNode(), ((Text) node).getNodeValue(), 0) - 1;
            int x1 = 0;
            if ((maxCallDepth == -1 || maxCallDepth >= cnt)) {
                Node tmp = node.getParentNode().getParentNode().getFirstChild().getNextSibling().getNextSibling().getNextSibling().getFirstChild();
                svg += "<rect id='" + y + "' onmouseover='msOver(evt)' onclick=\"msClick('" + y + "', '" + ((Text) node).getNodeValue().replaceAll(packageTrimPattern, "") + "', '" + ((Text) tmp).getNodeValue().replaceAll("<", "&lt;").replaceAll(">", "&gt;").replaceAll(packageTrimPattern, "") + "')\" style='fill: white' height='" + (((depth - 1) * 2) + 1) * 20 + "' width='15' y='" + ((((2 * y) + 1) * 20) + 45) + "' x='" + (((classes.indexOf(((Text) node).getNodeValue()) * nodeSep) + 120) + (x * 10)) + "'/>\n";
            }
            if (temp instanceof Text && classes.indexOf(((Text) temp).getNodeValue()) < classes.indexOf(((Text) node).getNodeValue())) {
                if ((maxCallDepth == -1 || maxCallDepth >= cnt) && (minCallDepth == -1 || minCallDepth <= cnt)) {
                    try {
                        x1 = traverse(temp.getParentNode().getParentNode(), ((Text) temp).getNodeValue(), 0) - 1;
                    } catch (Exception e) {
                    }
                    svg += "<line x1='" + (classes.indexOf(((Text) temp).getNodeValue()) >= 0 ? ((classes.indexOf(((Text) temp).getNodeValue()) * nodeSep) + 135 + (x1 * 10)) : 0) + "' y1='" + ((((2 * y) + 1) * 20) + 45) + "' x2='" + (((classes.indexOf(((Text) node).getNodeValue()) * nodeSep) + 120) + (x * 10)) + "' y2='" + ((((2 * y) + 1) * 20) + 45) + "'/>\n";
                    svg += "<path fill='black' stroke='none' marker-end='url(#mArrow)' style='fill: black; stroke: none' d='M " + (((classes.indexOf(((Text) node).getNodeValue()) * nodeSep) + 110) + (x * 10)) + " " + ((((2 * y) + 1) * 20) + 40) + " " + (((classes.indexOf(((Text) node).getNodeValue()) * nodeSep) + 120) + (x * 10)) + " " + ((((2 * y) + 1) * 20) + 45) + " " + (((classes.indexOf(((Text) node).getNodeValue()) * nodeSep) + 110) + (x * 10)) + " " + ((((2 * y) + 1) * 20) + 50) + "'/>";
                    Node tmp = node.getParentNode().getParentNode().getFirstChild().getNextSibling().getNextSibling().getNextSibling().getFirstChild();
                    svg += "<text font-family='Arial' font-size='9' x='" + (classes.indexOf(((Text) temp).getNodeValue()) >= 0 ? ((classes.indexOf(((Text) temp).getNodeValue()) * nodeSep) + 135 + (x1 * 10) + 3) : 3) + "' y='" + ((((2 * (y)) + 1) * 20) + 39) + "'><![CDATA[" + ((Text) tmp).getNodeValue().replaceAll(packageTrimPattern, "") + "]]></text>\n";
                    if(loop > 1)
                        svg += "<text font-family='Arial' fill='Red' font-size='8' x='" + (classes.indexOf(((Text) temp).getNodeValue()) >= 0 ? ((classes.indexOf(((Text) temp).getNodeValue()) * nodeSep) + 135 + (x1 * 10) + 3) : 3) + "' y='" + ((((2 * (y)) + 1) * 20) + 30) + "'><![CDATA[loop " + loop + "]]></text>\n";
                }
            } else if (temp instanceof Text) {
                if ((maxCallDepth == -1 || maxCallDepth >= cnt) && (minCallDepth == -1 || minCallDepth <= cnt)) {
                    try {
                        x1 = traverse(temp.getParentNode().getParentNode(), ((Text) temp).getNodeValue(), 0) - 1;
                    } catch (Exception e) {
                    }
                    svg += "<line x1='" + ((classes.indexOf(((Text) temp).getNodeValue()) * nodeSep) + 135 + (x1 * 10)) + "' y1='" + ((((2 * (y)) + 1) * 20) + 35) + "' x2='" + ((classes.indexOf(((Text) temp).getNodeValue()) * nodeSep) + 135 + (x1 * 10) + 35) + "' y2='" + ((((2 * (y)) + 1) * 20) + 35) + "' />\n";
                    svg += "<line x1='" + ((classes.indexOf(((Text) temp).getNodeValue()) * nodeSep) + 135 + (x1 * 10) + 35) + "' y1='" + ((((2 * (y)) + 1) * 20) + 35) + "' x2='" + ((classes.indexOf(((Text) temp).getNodeValue()) * nodeSep) + 135 + (x1 * 10) + 35) + "' y2='" + ((((2 * (y)) + 1) * 20) + 55) + "' />\n";
                    svg += "<line x1='" + ((classes.indexOf(((Text) temp).getNodeValue()) * nodeSep) + 135 + (x1 * 10) + 35) + "' y1='" + ((((2 * (y)) + 1) * 20) + 55) + "' x2='" + ((classes.indexOf(((Text) node).getNodeValue()) * nodeSep) + 135 + (x * 10)) + "' y2='" + ((((2 * (y)) + 1) * 20) + 55) + "' />\n";
                    svg += "<path fill='black' stroke='none' marker-end='url(#mArrow)' style='fill: black; stroke: none' d='M " + ((classes.indexOf(((Text) node).getNodeValue()) * nodeSep) + 145 + (x * 10)) + " " + ((((2 * (y)) + 1) * 20) + 50) + " " + ((classes.indexOf(((Text) node).getNodeValue()) * nodeSep) + 135 + (x * 10)) + " " + ((((2 * (y)) + 1) * 20) + 55) + " " + ((classes.indexOf(((Text) node).getNodeValue()) * nodeSep) + 145 + (x * 10)) + " " + ((((2 * (y)) + 1) * 20) + 60) + "'/>";
                    Node tmp = node.getParentNode().getParentNode().getFirstChild().getNextSibling().getNextSibling().getNextSibling().getFirstChild();
                    svg += "<text font-family='Arial' font-size='9' x='" + ((classes.indexOf(((Text) temp).getNodeValue()) * nodeSep) + 135 + (x1 * 10) + 38) + "' y='" + ((((2 * (y)) + 1) * 20) + 45) + "'><![CDATA[" + ((Text) tmp).getNodeValue().replaceAll(packageTrimPattern, "") + "]]></text>\n";
                    if(loop > 1)
                        svg += "<text font-family='Arial' fill='Red' font-size='8' x='" + ((classes.indexOf(((Text) temp).getNodeValue()) * nodeSep) + 135 + (x1 * 10) + 38) + "' y='" + ((((2 * (y)) + 1) * 20) + 35) + "'><![CDATA[loop " + loop + "]]></text>\n";
                }
            }
            y++;
        }
    }
}
