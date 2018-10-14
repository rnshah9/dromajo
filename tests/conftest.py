import pytest

def pytest_addoption(parser):
    parser.addoption("--output-dir",
                     required=True,
                     help="Test output folder")
    parser.addoption("--vharness",
                     required=True,
                     help="Path to vharness")

@pytest.fixture
def output_dir(request):
    return request.config.getoption("--output-dir")

@pytest.fixture
def vharness(request):
    return request.config.getoption("--vharness")
