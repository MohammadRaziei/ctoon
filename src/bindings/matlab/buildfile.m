function plan = buildfile
%BUILDFILE  MATLAB Build Tool plan for the CToon MATLAB MEX binding.
%
%   Usage:
%       buildtool                  % default: mex
%       buildtool mex              % compile ctoon_mex
%       buildtool test             % mex + run tests + HTML coverage
%       buildtool coverage         % mex + run tests + HTML + lcov + Cobertura XML
%       buildtool clean            % remove BUILD_DIR and BUILD_COVERAGE_DIR
%       buildtool -tasks           % list tasks
%
%   Configurable via global variables:
%
%       global BUILD_DIR           output directory for MEX binary and .m wrappers
%                                  (default: directory of this buildfile)
%       global BUILD_COVERAGE_DIR  coverage report output directory
%                                  (default: BUILD_DIR/coverage)
%       global MEX_SOURCES_PATH    directory containing ctoon.c
%                                  (default: <repo>/src)
%       global MEX_INCLUDE_DIR     directory containing ctoon.h
%                                  (default: <repo>/include)
%
%   CMake invocation pattern:
%
%       matlab -sd src/bindings/matlab -batch \
%         "global BUILD_DIR BUILD_COVERAGE_DIR MEX_SOURCES_PATH MEX_INCLUDE_DIR; \
%          BUILD_DIR='${CMAKE_CURRENT_BINARY_DIR}'; \
%          BUILD_COVERAGE_DIR='${CTOON_COVERAGE_MATLAB_BINARY_DIR}'; \
%          MEX_SOURCES_PATH='${PROJECT_SOURCE_DIR}/src'; \
%          MEX_INCLUDE_DIR='${PROJECT_SOURCE_DIR}/include'; \
%          buildtool mex"
%
%   Coverage output layout (in BUILD_COVERAGE_DIR):
%       coverage.lcov        lcov tracefile
%       html/index.html      HTML report
%       cobertura.xml        Cobertura XML  (CI / SonarQube)
%
%   Requirements:
%       MATLAB R2022b+  (Build Tool)
%       MATLAB R2024a+  (addCodeCoverage)
%       R2014b+         for mex compilation via ctoon_build
%
%   See also: buildtool, ctoon_build, ctoon_install,
%             matlab.buildtool.tasks.TestTask.

import matlab.buildtool.tasks.TestTask

global BUILD_DIR BUILD_COVERAGE_DIR

here    = fileparts(mfilename('fullpath'));
testDir = fullfile(here, '..', '..', '..', 'tests', 'matlab');

% ---- resolve paths -------------------------------------------------------
if isempty(BUILD_DIR)
    buildDir = here;
else
    buildDir = strtrim(BUILD_DIR);
end

if isempty(BUILD_COVERAGE_DIR)
    covDir = fullfile(buildDir, 'coverage');
else
    covDir = strtrim(BUILD_COVERAGE_DIR);
end

covHtmlDir = fullfile(covDir, 'html');
covXmlFile = fullfile(covDir, 'cobertura.xml');

% ---- build plan ----------------------------------------------------------
plan = buildplan(localfunctions);

% "test" — run tests + HTML coverage report
plan("test") = TestTask(testDir, SourceFiles=buildDir) ...
    .addCodeCoverage(fullfile(covHtmlDir, 'index.html'));

% "coverage" — depends on test; coverageTask also writes lcov + Cobertura
plan("coverage").Dependencies = "test";

plan("test").Dependencies  = "mex";
plan("clean").Dependencies = {};

plan.DefaultTasks = "mex";

end % buildfile


% =========================================================================
% Local task functions
% =========================================================================

function mexTask(~)
%MEX  Compile ctoon_mex via ctoon_build.

here = fileparts(mfilename('fullpath'));
run(fullfile(here, 'ctoon_build.m'));

end % mexTask


function coverageTask(~)
%COVERAGE  Write Cobertura XML and lcov tracefile from test coverage data.
%
%   "test" has already run the tests and written the HTML report.
%   This task converts the results to Cobertura XML (via TestRunner) and
%   lcov format (via matlab.coverage.Result on R2024b+).

global BUILD_DIR BUILD_COVERAGE_DIR

here = fileparts(mfilename('fullpath'));

if isempty(BUILD_DIR)
    buildDir = here;
else
    buildDir = strtrim(BUILD_DIR);
end

if isempty(BUILD_COVERAGE_DIR)
    covDir = fullfile(buildDir, 'coverage');
else
    covDir = strtrim(BUILD_COVERAGE_DIR);
end

covXmlFile  = fullfile(covDir, 'cobertura.xml');
covLcovFile = fullfile(covDir, 'coverage.lcov');
covMatFile  = fullfile(covDir, 'coverage.mat');

if ~isfolder(covDir)
    mkdir(covDir);
end

testDir = fullfile(here, '..', '..', '..', 'tests', 'matlab');

import matlab.unittest.TestRunner
import matlab.unittest.plugins.CodeCoveragePlugin
import matlab.unittest.plugins.codecoverage.CoberturaFormat

suite = testsuite(testDir);

% ---- Cobertura XML -------------------------------------------------------
runner = TestRunner.withNoPlugins;
runner.addPlugin(CodeCoveragePlugin.forFolder(buildDir, ...
    Producing=CoberturaFormat(covXmlFile)));
results = runner.run(suite);
if any([results.Failed])
    error('coverageTask:testsFailed', 'Tests failed during coverage run.');
end
fprintf('  Cobertura XML → %s\n', covXmlFile);

% ---- lcov tracefile (R2024b+) -------------------------------------------
try
    import matlab.unittest.plugins.codecoverage.CoverageResult
    runner2 = TestRunner.withNoPlugins;
    runner2.addPlugin(CodeCoveragePlugin.forFolder(buildDir, ...
        Producing=CoverageResult(covMatFile)));
    runner2.run(suite);
    result = load(covMatFile).result;
    write(result, covLcovFile);
    delete(covMatFile);
    fprintf('  lcov tracefile  → %s\n', covLcovFile);
catch
    % Fallback: minimal valid stub for older releases
    fid = fopen(covLcovFile, 'w');
    if fid ~= -1
        fprintf(fid, 'TN:\nend_of_record\n');
        fclose(fid);
    end
    fprintf('  lcov stub       → %s  (R2024b+ needed for full lcov)\n', ...
            covLcovFile);
end

end % coverageTask


function cleanTask(~)
%CLEAN  Remove BUILD_DIR contents and BUILD_COVERAGE_DIR.

global BUILD_DIR BUILD_COVERAGE_DIR

here = fileparts(mfilename('fullpath'));

if isempty(BUILD_DIR)
    buildDir = here;
else
    buildDir = strtrim(BUILD_DIR);
end

if isempty(BUILD_COVERAGE_DIR)
    covDir = fullfile(buildDir, 'coverage');
else
    covDir = strtrim(BUILD_COVERAGE_DIR);
end

cleaned = false;

if ~strcmp(buildDir, here) && isfolder(buildDir)
    rmdir(buildDir, 's');
    fprintf('  Removed: %s\n', buildDir);
    cleaned = true;
elseif strcmp(buildDir, here)
    % Only remove the MEX binary, not the source files
    mexFile = fullfile(here, ['ctoon_mex.' mexext]);
    if isfile(mexFile)
        delete(mexFile);
        fprintf('  Removed: %s\n', mexFile);
        cleaned = true;
    end
end

if isfolder(covDir)
    rmdir(covDir, 's');
    fprintf('  Removed: %s\n', covDir);
    cleaned = true;
end

if ~cleaned
    fprintf('  Nothing to clean.\n');
end

end % cleanTask
