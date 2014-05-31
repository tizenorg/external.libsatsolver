#!/usr/bin/env ruby
#  -*- mode: ruby; fill-column: 78 -*-
#
# runtest.rb
#
# ruby script to run zypp/libredcarpet testcases
#
# Usage:
#  runtest.rb <testcase1> [... <testcaseN>]
#     run one or more test cases
#  runtest.rb [-r] <dir>
#     run all test cases (*test.xml) below a specific directory
#     if -r is given, recursively descend to sub-directories
#

require 'test/unit'
require 'pp'
require 'getoptlong'

$verbose = false
$redcarpet = false

$tests = Array.new
$builddir= File.join( Dir.getwd, "../..")
$sourcedir = File.join( Dir.getwd, "../..")

$fails = Array.new
$ignorecount = 0

class CompareResult
  Incomplete = 0
  KnownFailure = 1
  UnexpectedFailure = 2
  KnownPass = 3
  UnexpectedPass = 4
end

def known_failure? name
  $fails.each { |f|
     if name.length < f.length
        next
     end
     if name[-f.length..-1] == f
        return true
     end
  }
  return false
end

class Solution

  # poor mans diff
  def Solution.filediff name1, name2
    begin
      f1 = File.new( name1, "r" )
    rescue
      STDERR.puts "Cannot open #{name1}"
      return false
    end
    begin
      f2 = File.new( name2, "r" )
    rescue
      STDERR.puts "Cannot open #{name2}"
      return false
    end
    a1 = f1.readlines
    a2 = f2.readlines
    i = 0
    a1.each { |l1|
      if (l1 =~ /unflag/)
	next
      end
      l2 = a2[i]
      if (l2 =~ /unflag/)
	i += 1
	retry
      end
      if l2
	if (l1 != l2)
	  puts "- #{l1}"
	  puts "+ #{l2}"
	end
      else
#	puts "- #{l1}"
      end
      i += 1
    }
    while i < a2.size
      puts "+ #{a2[i]}"
      i += 1
    end
  end

  def Solution.read fname
    solutions = Array.new
    solution = nil
    problemFound = false
    
    # read solution and filter irrelevant lines
    IO.foreach( fname ) { |l|
      l = l + "\n"
      case l.chomp!
      when /Solution \#([0-9]+):/
	if solution && !solution.empty?
	   if problemFound
	      solutions << solution # no sorting
           else
	      solutions << solution.sort!
	   end 
        end
	solution = Array.new
	problemFound = false
      when /Problem ([0-9]+):/
	if solution && !solution.empty?
	   if problemFound
	      solutions << solution # no sorting
           else
	      solutions << solution.sort!
	   end 
        end
	solution = Array.new
	problemFound = true
      when /> install /, /> upgrade /, /> remove /
	STDERR.puts "No 'Solution' in #{fname}" unless solution
	solution << l
      else
	if problemFound
	   solution << l.lstrip
        end
      end
    }
    
    # last entry insert
    if solution && !solution.empty?
       if problemFound
	      solutions << solution # no sorting
       else
	      solutions << solution.sort!
       end 
    end

    solutions
  end

  # compare solution s with result r
  def Solution.compare sname, rname
    unless File.readable?( sname )
      return CompareResult::Incomplete
    end
    unless File.readable?( rname )
      STDERR.puts "Cannot open #{rname}"
      return CompareResult::Incomplete
    end

    solutions = Solution.read sname
    results = Solution.read rname

    if (solutions.empty? && results.empty?)
      if ( known_failure?( rname ) )
	STDERR.puts "#{rname} passed"
	return CompareResult::UnexpectedPass
      else
	return CompareResult::KnownPass  
      end
    end

    # solutions,results is an array of sulutions AND problems
    # As the SAT solver does returns only one sulution and one or more
    # problems ALL these entries has to fit to the given solution

    if solutions.size == results.size
       solutionFit = false
       results.each { |r|
          solutionFit = false
          solutions.each { |s|
	     if s == r
	        solutionFit = true
                break
             end
          }
       }
       if solutionFit
	   if ( known_failure?( rname ) )
	     STDERR.puts "#{rname} passed"
	     return CompareResult::UnexpectedPass
	   else
	     return CompareResult::KnownPass  
	   end
       end
    else
       if $verbose
         puts "The number of results does not fit to the number of given solutions."
       end
    end

    if ( known_failure?( rname ) )
      return CompareResult::KnownFailure
    end

    case sname
    when /\.solution([2-9]+)$/
      # we print error with solution1
      return CompareResult::UnexpectedFailure
    end

    STDERR.puts "#{rname} failed"
    if $verbose
	Solution.filediff( sname, rname)
#      system( "diff -U 0 #{sname} #{rname}")
    end
    return CompareResult::UnexpectedFailure
  end

end


class Tester < Test::Unit::TestCase
  
  def test_run
    upassed = 0
    epassed = 0
    ufailed = 0
    efailed = 0
    puts "#{$tests.size} tests ahead"
    $tests.sort!
    $tests.each { |test|
      if $verbose
         puts "Testing #{test}"
      end
      basename = File.basename(test, ".xml")
      #print "."
      #STDOUT.flush
      dir = File.dirname(test)
      args = ""
      args = "--redcarpet" if $redcarpet
      if ( system( "#{$deptestomatic} #{args} #{dir}/#{basename}.xml > #{dir}/#{basename}.result" ) )
        sname = File.join( dir, "#{basename}.solution" )
        rname = File.join( dir, "#{basename}.result" )
	result = Solution.compare( sname, rname )
	if result == CompareResult::Incomplete
	  sname = File.join( dir, "#{basename}.solution3" )
	  result = Solution.compare( sname, rname )
	  case result
	  when CompareResult::UnexpectedFailure, CompareResult::KnownFailure, CompareResult::Incomplete
	    sname = File.join( dir, "#{basename}.solution2" )
	    result = Solution.compare( sname, rname )
	  end
	  case result
	  when CompareResult::UnexpectedFailure, CompareResult::KnownFailure, CompareResult::Incomplete
	    sname = File.join( dir, "#{basename}.solution1" )
	    result = Solution.compare( sname, rname )
	  end
	end
	if result == CompareResult::Incomplete
	  puts "#{test} is incomplete"
	end
	case result
	when CompareResult::UnexpectedFailure
	  ufailed += 1
	when CompareResult::UnexpectedPass
	  upassed += 1
	when CompareResult::KnownFailure
	  efailed += 1
	when CompareResult::KnownPass
	  epassed += 1
	end
#        assert(  )
#      puts "(cd #{File.dirname(test)}; #{$deptestomatic} #{basename}.xml > #{basename}.result)" 
      else
	puts "#{test} is incomplete"
      end
    }
    puts "\n\t==> #{$tests.size} tests: (exp:#{epassed}/unexp:#{upassed}) passed, (exp:#{efailed}/unexp:#{ufailed}) failed, #{$ignorecount} ignored <==\n"
    assert(ufailed == 0, 'Unexpected failures')
    assert(upassed == 0, 'Unexpected passes')
  end
end

class Runner
 
  def run wd, arg, recurse=nil
    fullname = arg
    if wd: 
       fullname = File.join( wd, arg )
    end
    #puts "Examine #{fullname}"
    if File.directory?( fullname )
      rundir( fullname, recurse ) 
    elsif (arg =~ /test.xml$/)
         #puts "Run #{fullname}"
      $tests << fullname
    end
  end
  
  # process current directory
  #
  def rundir path, recurse
    #puts "Rundir #{path}"
    ignores = Array.new
    ignorefile = File.join( path, "ignore" )
    if File.readable?( ignorefile )
      IO.foreach( ignorefile ) { |line|
	line.chomp!
	if ( line !~ /^\s/ && line !~ /^#/ )
	  ignores << line
	end
      }
    end
    
    dir = Dir.new( path )

    dir.each { |name|
      if File.directory?( name )
	rundir File.join( path, name ), recurse if recurse
      else
	#puts name, ignores.member?(name)
	if !ignores.member?(name)
	  run path, name
	else
	  $ignorecount += 1
	end
      end
    }
    
  end
  
end

class Recurse
  def initialize dir
    raise "<origin> is not a directory" unless File.directory?( dir )
    @from = Dir.new( dir )               # absolute path to origin
    @path = ""                                # relative path within
  end

private
  
  def check_name name
    case name
    when /-test\.xml$/
      "cat"
    else
      "|../tools/helix2solv"
    end
  end
  
  def process_file name
    dots = name.split "."
    #return if dots.size < 2
    cmd = nil
    srcdir = File.join( @from.path, @path )
    srcname = File.join( srcdir, name )
    suffix = nil
    case dots.last
    when "bz2"
      chk = check_name name
      return unless chk
      cmd = "bzcat #{srcname} "
      cmd += chk
      suffix = "solv"
      name = File.basename name, ".bz2"
    when "gz"
      chk = check_name name
      return unless chk
      cmd = "zcat #{srcname} "
      cmd += chk
      suffix = "solv"
      name = File.basename name, ".gz"
    when "xml"
      cmd = check_name name
      return unless cmd
      if cmd[0,1] == "|"
	cmd = "cat #{srcname} " + cmd
	suffix = "solv"
      else
        return
      end
    else
      return
    end
    destname = File.basename name, ".*"
    destname += ".#{suffix}" if suffix
    cmd += " > "
    fulldest = File.join( srcdir, destname )
    cmd += fulldest
    return if File.exists?( fulldest ) && File.mtime( fulldest ) >= File.mtime( srcname )
    print "Generating #{fulldest}\n"
    puts "*** FAILED: #{fulldest}" unless system cmd
  end
  
  def process_dir name
    return if name[0,1] == "."
    #print "#{name} "
    STDOUT.flush
    start = @path
    @path = File.join( @path, name )
    process
    @path = start
  end

public
  # process current directory
  #
  def process
    from = Dir.new( File.join( @from.path, @path ) )
    
    from.each { |fname|
      if File.directory?( File.join( from.path, fname ) )
	process_dir fname
      else
	process_file fname
      end
    }
    
  end
  
end

def recurse path
  return unless File.directory?( path )
  dir = Dir.new path
  dir.each{ |fname|
    next if fname[0,1] == '.'
    fullname = dir.path + "/" + fname
    if File.directory?( fullname )
      #puts "Dir #{fullname}"
      next unless recursive
      if tags.include?( fname )
        $exitcode = 1
	STDERR.puts "Directory #{fname} already seen, symlink loop ?"
	next
      end
      tags.push fname.downcase
      import( [ Dir.new( fullname ), recursive, tags ] )
      tags.pop
    elsif File.file?( fullname )
      #puts "File #{fullname}"
      args = [ "-t" ]
      args << tags.join(",")
      args << fullname
      add( args )
    else
      $exitcode = 1
      STDERR.puts "Unknown file #{fullname} : #{File.stat(fullname).ftype}, skipping"
    end
  }															    
end

#----------------------------

def usage err=nil
  STDERR.puts "** Error: #{err}" if err
  STDERR.puts "Usage:"
  STDERR.puts "\truntest.rb <testcase1> [... <testcaseN>]"
  STDERR.puts "\t\trun one or more test cases"
  STDERR.puts "\truntest.rb [-r] <dir>"
  STDERR.puts "\t\trun all test cases (*test.xml) below a specific directory"
  STDERR.puts "\t\tif -r is given, recursively descend to sub-directories"
  exit 1
end

#------
# main

puts "Running in #{Dir.getwd}"

opts = GetoptLong.new(
      [ '--help', '-h', GetoptLong::NO_ARGUMENT ],
      [ '--redcarpet', GetoptLong::NO_ARGUMENT ],
      [ '-r', GetoptLong::NO_ARGUMENT ],
      [ '-v', GetoptLong::NO_ARGUMENT ],
      [ '-s', GetoptLong::OPTIONAL_ARGUMENT ],
      [ '-b', GetoptLong::OPTIONAL_ARGUMENT ]
    )

recurse = false

opts.each do |opt, arg|
      case opt
        when '--help'
          usage
        when '--redcarpet'
          $redcarpet = true
        when '-r'
          recurse = true
        when '-v'
          $verbose = true
        when '-s'
          $sourcedir = arg 
        when '-b'
          $builddir = arg 
      end
    end

$deptestomatic = File.join( $builddir, "tests", "solver", "deptestomatic" )
raise "Cannot find '#{$deptestomatic}' executable. Please use -b to pass builddir path" unless File.executable?( $deptestomatic )
$readmefile=File.join( $sourcedir, "tests", "solver", "README.FAILS")
raise "Cannot find '#{$readmefile}' file. Please use -s to pass sourcedir path" unless File.readable?( $readmefile )

IO.foreach( $readmefile ) { |line|
  line.chomp
  if ( line !~ /^\s/ )
    line = line[0..-6] + ".result"
    $fails << line
  end
}

#preproc = Recurse.new Dir.getwd
#preproc.process

r = Runner.new

ARGV.each { |arg|
  wd = "." unless arg[0,1] == "/"
  r.run wd, arg, recurse
}

