#!/usr/bin/ruby

# tree node:
# { 
#   key: node_key,
#   metadata: [class_name, method_name, method_signature],
#   order: sort_order,
#   children: [],
# }


def print_tree(node, level)
  return if node.nil?
  cnodes = node[:children]
  return if cnodes.length == 0

  puts "  "*level+node[:metadata].join(", ")
  cnodes.each do |child_node|
    print_tree(child_node, level+1)
  end
end

def print_tree_xml(node, level)
  return if node.nil?
  cnodes = node[:children]
  cn, mn, ms = node[:metadata]
  puts "  "*level + "<call>"
  puts "  "*level + " <class><![CDATA[#{cn}]]></class>"
  puts "  "*level + " <method><![CDATA[#{mn} #{ms}]]></method>"
  cnodes.each do |child_node|
    print_tree_xml(child_node, level+1)
  end
  puts "  "*level + "</call>"
end

def print_tree(node, level)
  return if node.nil?
  cnodes = node[:children]
  return if cnodes.length == 0

  puts "  "*level+node[:metadata].join(", ")
  cnodes.each do |child_node|
    print_tree(child_node, level+1)
  end
end

threads = {}

subtrees = {}

File.open("threads.out").readlines.map {|l| l.chomp}.each do |tid|

  thread_subtree = []
  thread_node = {
    :key => tid,
    :metadata => ["", "", ""],
    :order => 1,
    :children => thread_subtree
  }
  threads[tid] = thread_node
  subtrees[tid] = thread_subtree
end

File.open("edges.out").readlines.each do |line|
  nkey, pkey, order, mn, ms, cn = line.chomp.split
  order = order.to_i

  this_node = {
    :key => nkey,
    :metadata => [cn, mn, ms],
    :order => order,
    :children => []
  }

  parent_tree_nodes = subtrees[pkey]
  if parent_tree_nodes.nil?
    parent_tree_nodes = []
    subtrees[pkey] = parent_tree_nodes
  end

  parent_tree_nodes.push(this_node)

  subtrees[nkey] = this_node[:children]
end


threads.each do |k,v|
  # next if k != "15172"
  puts "<Thread id=\"#{k}\""
  # print_tree(v, 0)
  print_tree_xml(v, 1)
  puts "</Thread>"
end
