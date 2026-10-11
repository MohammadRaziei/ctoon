function run_tests(moxunitDir, buildDir, junitFile)
%RUN_TESTS  Run the CToon Octave test suite with MOxUnit and exit(0|1).
%
%   run_tests(moxunitDir, buildDir)
%   run_tests(moxunitDir, buildDir, junitFile)
%
%   moxunitDir  root of a MOxUnit checkout (the folder that contains the
%               inner MOxUnit/ folder with moxunit_runtests.m)
%   buildDir    folder produced by ctoon_build: ctoon_mex.mex + +ctoon/
%   junitFile   optional path for a JUnit XML report
%
%   Meant to be run non-interactively; the process exit status is the result:
%     octave-cli --norc --quiet --eval ...
%       "addpath('tests/octave'); run_tests('<MOxUnit>', '<build>')"
%
%   Nothing here compiles anything: the binding must already be built.

if nargin < 3, junitFile = ''; end

% moxunit_runtests changes directory while it collects tests, so every path
% has to be absolute or relative arguments stop resolving half-way through.
moxunitDir = make_absolute_filename(moxunitDir);
buildDir   = make_absolute_filename(buildDir);
if ~isempty(junitFile), junitFile = make_absolute_filename(junitFile); end

% Octave's built-in load() is shadowed by ctoon.load (Python-style alias).
warning('off', 'Octave:shadowed-function');

addpath(fullfile(moxunitDir, 'MOxUnit'));
moxunit_set_path();

if ~exist(fullfile(buildDir, ['ctoon_mex.' mexext]), 'file')
    fprintf(2, 'run_tests: no ctoon_mex.%s in %s -- build the binding first.\n', mexext, buildDir);
    exit(2);
end
addpath(buildDir);

testsDir = fileparts(mfilename('fullpath'));
args = {testsDir, '-verbose'};
if ~isempty(junitFile)
    args = [args, {'-junit_xml_file', junitFile}];
end

ok = moxunit_runtests(args{:});
exit(double(~ok));
end
