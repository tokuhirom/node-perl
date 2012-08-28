import Utils
import Options

srcdir = '.'
blddir = 'build'
VERSION = '0.0.1'

def set_options(opt): 
    opt.tool_options('compiler_cxx')
    opt.add_option('--perl', action='store', default='perl', help='perl path')

def configure(conf):
    conf.check_tool('compiler_cxx')
    conf.check_tool('node_addon')
    perl = Options.options.perl
    print('→ the value of perl is %r' % perl)
    conf.env.append_unique('CXXFLAGS',Utils.cmd_output(perl + ' -MExtUtils::Embed -e ccopts').split())
    conf.env.append_unique('CXXFLAGS',['-Duseithreads'])
    conf.env.append_unique('LINKFLAGS',Utils.cmd_output(perl + ' -MExtUtils::Embed -e ldopts').split())
    conf.env.append_unique('LDFLAGS',['-static'])

def build(bld):
    obj = bld.new_task_gen('cxx', 'shlib', 'node_addon')
    obj.target = 'perl-simple'
    obj.source = './src/perlxsi.cc ./src/perl_bindings.cc'

