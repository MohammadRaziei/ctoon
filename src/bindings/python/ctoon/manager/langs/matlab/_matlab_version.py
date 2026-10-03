# Verbatim copy of MohammadRaziei's tested gist:
# https://gist.github.com/MohammadRaziei/9c17c3f6102c166423190af26c0c0cda
# (matlab_version.py). Keep the code below unchanged; build on it from
# matlab.py instead of editing it here.
import os
import shutil
import platform
import xml.etree.ElementTree as ET


def find_matlab_executable() -> str:
    """Locates the MATLAB executable across different operating systems.

    The function first checks the system PATH environment variable. If not found,
    it scans default and custom installation directories for Windows, Linux,
    and macOS, automatically prioritizing the most recent version available.

    Returns:
        str: The absolute real path to the MATLAB executable.

    Raises:
        FileNotFoundError: If the MATLAB executable cannot be found in the
            system PATH or any target installation directories.
    """
    # 1. Search in system PATH
    matlab_str_path = shutil.which("matlab")
    if matlab_str_path:
        return os.path.realpath(matlab_str_path)

    # 2. Map OS to potential installation root directories and relative binary paths
    os_configs = {
        "Windows": (
            [
                r"C:\Program Files\MATLAB",
                r"C:\Program Files (x86)\MATLAB",
                r"C:\MATLAB",
            ],
            os.path.join("bin", "matlab.exe"),
        ),
        "Linux": (
            ["/usr/local/MATLAB", os.path.expanduser("~/MATLAB"), "/opt/MATLAB"],
            os.path.join("bin", "matlab"),
        ),
        "Darwin": ( # macOS
            ["/Applications", os.path.expanduser("~/Applications")],
            os.path.join("bin", "matlab"),
        ),
    }

    system = platform.system()
    if system in os_configs:
        base_dirs, rel_bin_path = os_configs[system]

        # Iterate through candidate directories
        for base_dir in base_dirs:
            if not os.path.exists(base_dir):
                continue

            # Scan and sort folders in reverse order to prioritize the latest release
            for folder in sorted(os.listdir(base_dir), reverse=True):
                # Safety check for standard macOS App Bundle naming patterns
                if system == "Darwin" and not (
                    folder.startswith("MATLAB_") and folder.endswith(".app")
                ):
                    continue

                exe_path = os.path.join(base_dir, folder, rel_bin_path)
                if os.path.exists(exe_path):
                    return os.path.realpath(exe_path)

    raise FileNotFoundError(
        "MATLAB executable could not be found in system PATH or default directories."
    )


def get_matlab_release() -> str:
    """Determines the MATLAB release name by parsing its VersionInfo.xml file.

    This method resolves the root installation path from the found executable
    and extracts the release tag without starting the heavy MATLAB runtime engine.

    Returns:
        str: The release identifier name (e.g., 'R2024a').

    Raises:
        FileNotFoundError: If the 'VersionInfo.xml' file cannot be located.
        RuntimeError: If XML parsing fails or the release tag is missing.
    """
    executable_path = find_matlab_executable()

    # Resolve the main installation directory (parent directory of 'bin')
    matlab_root = os.path.dirname(os.path.dirname(executable_path))

    # Handle standard macOS App Bundle structure deviations
    if platform.system() == "Darwin":
        xml_path = os.path.join(matlab_root, "Contents", "VersionInfo.xml")
    else:
        xml_path = os.path.join(matlab_root, "VersionInfo.xml")

    if not os.path.exists(xml_path):
        raise FileNotFoundError(
            f"Could not find 'VersionInfo.xml' at target path: {xml_path}"
        )

    # Parse the XML structure safely
    try:
        tree = ET.parse(xml_path)
        root = tree.getroot()

        # Uses './/' syntax to capture the tag regardless of active XML Namespaces
        release_element = root.find(".//release")

        if release_element is not None and release_element.text:
            return release_element.text.strip()

        raise RuntimeError(
            "The <release> tag is missing or empty inside VersionInfo.xml."
        )

    except ET.ParseError as error:
        raise RuntimeError(f"Failed to accurately parse VersionInfo.xml: {error}")


if __name__ == "__main__":
    try:
        release_name = get_matlab_release()
        print(f"Detected MATLAB Release: {release_name}")
    except Exception as runtime_error:
        print(f"Error: {runtime_error}")
