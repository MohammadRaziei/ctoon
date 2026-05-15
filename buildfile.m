function plan = buildfile
%BUILDFILE  MATLAB Build Tool plan for CToon MATLAB MEX binding.
%
%   Run from repository root via matlab-actions/run-build or locally:
%
%       buildtool                   % default: build
%       buildtool build             % compile ctoon_mex
%       buildtool test              % build + run tests + HTML coverage
%       buildtool coverage          % build + tests + HTML + lcov
%       buildtool install           % build + install to MATLAB path
%       buildtool clean             % remove BUILD_DIR and COVERAGE_OUTPUT_DIR
%       buildtool -tasks            % list all tasks
%
%   Global variables (optional - set before buildtool or via -batch):
%
%       global BUILD_DIR            output directory for MEX binary and wrappers
%                                   (default: <repo>/build/matlab)
%       global COVERAGE_OUTPUT_DIR  coverage report output directory
%                                   (default: BUILD_DIR/coverage)
%
%   Coverage output layout (in COVERAGE_OUTPUT_DIR):
%       coverage.lcov               lcov tracefile
%       html/index.html             HTML coverage report
%
%   GitHub Actions usage:
%       - uses: matlab-actions/run-build@v2
%         with:
%           tasks: build
%
%   CMake usage:
%       matlab -sd . -batch \
%         "global BUILD_DIR COVERAGE_OUTPUT_DIR; \
%          BUILD_DIR='${CMAKE_CURRENT_BINARY_DIR}'; \
%          COVERAGE_OUTPUT_DIR='${CTOON_COVERAGE_MATLAB_BINARY_DIR}'; \
%          buildtool build"
%
%   See also: buildtool, ctoon_build, ctoon_install.

import matlab.buildtool.tasks.TestTask

global BUILD_DIR COVERAGE_OUTPUT_DIR

here    = fileparts(mfilename('fullpath'));          % repo root
binding = fullfile(here, 'src', 'bindings', 'matlab');
testDir = fullfile(here, 'tests', 'matlab');

buildDir = resolve(BUILD_DIR, fullfile(here, 'build', 'matlab'));
covDir   = resolve(COVERAGE_OUTPUT_DIR, fullfile(buildDir, 'coverage'));

covHtmlDir = fullfile(covDir, 'html');

% ---- plan ----------------------------------------------------------------
plan = buildplan(localfunctions);

plan("test") = TestTask(testDir, SourceFiles=buildDir) ...
    .addCodeCoverage(fullfile(covHtmlDir, 'index.html'));

plan("test").Dependencies     = "build";
plan("coverage").Dependencies = "test";
plan("install").Dependencies  = "build";
plan("clean").Dependencies    = {};

plan.DefaultTasks = "build";

end


% =========================================================================
% Tasks
% =========================================================================

function buildTask(~)
%BUILD  Compile ctoon_mex and add to path.

global BUILD_DIR
here     = fileparts(mfilename('fullpath'));
buildDir = resolve(BUILD_DIR, fullfile(here, 'build', 'matlab'));
binding  = fullfile(here, 'src', 'bindings', 'matlab');

% Forward BUILD_DIR so ctoon_build picks it up
BUILD_DIR = buildDir;
run(fullfile(binding, 'ctoon_build.m'));
addpath(buildDir);

end


function coverageTask(~)
%COVERAGE  Write lcov tracefile and HTML report from test coverage data.
%
%   Output (in COVERAGE_OUTPUT_DIR):
%       coverage.lcov      lcov tracefile  (R2024b+; stub on older releases)
%       html/index.html    HTML report     (R2019b+)

global BUILD_DIR COVERAGE_OUTPUT_DIR

here     = fileparts(mfilename('fullpath'));
buildDir = resolve(BUILD_DIR, fullfile(here, 'build', 'matlab'));
covDir   = resolve(COVERAGE_OUTPUT_DIR, fullfile(buildDir, 'coverage'));
testDir  = fullfile(here, 'tests', 'matlab');

covLcovFile = fullfile(covDir, 'coverage.lcov');
covHtmlDir  = fullfile(covDir, 'html');
covMatFile  = fullfile(covDir, 'coverage.mat');

if ~isfolder(covDir),     mkdir(covDir);     end
if ~isfolder(covHtmlDir), mkdir(covHtmlDir); end

suite = testsuite(testDir);

% ---- HTML report - CoverageReport available since R2019b ----------------
runner = matlab.unittest.TestRunner.withNoPlugins;
runner.addPlugin(matlab.unittest.plugins.CodeCoveragePlugin.forFolder( ...
    buildDir, ...
    Producing=matlab.unittest.plugins.codecoverage.CoverageReport(covHtmlDir)));
results = runner.run(suite);
if any([results.Failed])
    error('coverageTask:testsFailed', 'Tests failed during coverage run.');
end
fprintf('  HTML report     -> %s
', fullfile(covHtmlDir, 'index.html'));

% ---- lcov tracefile - CoverageResult available since R2024b -------------
try
    runner2 = matlab.unittest.TestRunner.withNoPlugins;
    runner2.addPlugin(matlab.unittest.plugins.CodeCoveragePlugin.forFolder( ...
        buildDir, ...
        Producing=matlab.unittest.plugins.codecoverage.CoverageResult(covMatFile)));
    runner2.run(suite);
    result = load(covMatFile).result;
    write(result, covLcovFile);
    if isfile(covMatFile), delete(covMatFile); end
    fprintf('  lcov tracefile  -> %s
', covLcovFile);
catch
    % Fallback: minimal valid lcov stub so downstream tools don't fail
    fid = fopen(covLcovFile, 'w');
    if fid ~= -1
        fprintf(fid, 'TN:
end_of_record
');
        fclose(fid);
    end
    fprintf('  lcov stub       -> %s  (upgrade to R2024b for full lcov)
', covLcovFile);
end

end


function installTask(~)
%INSTALL  Install CToon MEX binding to MATLAB path permanently.

global BUILD_DIR
here     = fileparts(mfilename('fullpath'));
buildDir = resolve(BUILD_DIR, fullfile(here, 'build', 'matlab'));
binding  = fullfile(here, 'src', 'bindings', 'matlab');

BUILD_DIR = buildDir;
run(fullfile(binding, 'ctoon_install.m'));

end


function cleanTask(~)
%CLEAN  Remove BUILD_DIR and COVERAGE_OUTPUT_DIR.

global BUILD_DIR COVERAGE_OUTPUT_DIR

here     = fileparts(mfilename('fullpath'));
buildDir = resolve(BUILD_DIR, fullfile(here, 'build', 'matlab'));
covDir   = resolve(COVERAGE_OUTPUT_DIR, fullfile(buildDir, 'coverage'));

cleaned = false;

if isfolder(covDir)
    rmdir(covDir, 's');
    fprintf('  Removed: %s\n', covDir);
    cleaned = true;
end

if isfolder(buildDir)
    rmdir(buildDir, 's');
    fprintf('  Removed: %s\n', buildDir);
    cleaned = true;
end

if ~cleaned
    fprintf('  Nothing to clean.\n');
end

end


% =========================================================================
% Helper
% =========================================================================
function val = resolve(var, default)
if isempty(var)
    val = default;
else
    val = strtrim(var);
end
end
