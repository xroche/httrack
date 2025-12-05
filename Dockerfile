# First, specify the base Docker image.
# You can see the Docker images from Apify at https://hub.docker.com/r/apify/.
# You can also use any other image from Docker Hub.
FROM apify/actor-python:3.13

# Switch to root to install system packages
USER root

# Install HTTrack and required system dependencies
# Based on setup-wsl.sh and WSL_SETUP.md requirements
RUN echo "=== Installing HTTrack and system dependencies ===" \
 && apt-get update \
 && apt-get install -y --no-install-recommends \
    httrack \
    wget \
    curl \
    ca-certificates \
    zlib1g \
    libssl3 \
 && echo "=== Verifying HTTrack installation ===" \
 && httrack --version \
 && echo "=== Cleaning up apt cache ===" \
 && apt-get clean \
 && rm -rf /var/lib/apt/lists/* \
 && echo "=== HTTrack installation complete ==="

# Switch back to myuser for the rest of the build
USER myuser

# Create directory for scraped websites (output)
RUN mkdir -p /home/myuser/scraped_websites

# Second, copy just requirements.txt into the Actor image,
# since it should be the only file that affects the dependency install in the next step,
# in order to speed up the build
COPY --chown=myuser:myuser requirements.txt ./

# Install the packages specified in requirements.txt,
# Print the installed Python version, pip version
# and all installed packages with their versions for debugging
RUN echo "Python version:" \
 && python --version \
 && echo "Pip version:" \
 && pip --version \
 && echo "Installing dependencies:" \
 && pip install -r requirements.txt \
 && echo "All installed Python packages:" \
 && pip freeze

# Next, copy the remaining files and directories with the source code.
# Since we do this after installing the dependencies, quick build will be really fast
# for most source file changes.
COPY --chown=myuser:myuser . ./

# Copy scraper script and documentation
COPY --chown=myuser:myuser website_scraper.py ./
COPY --chown=myuser:myuser README_SCRAPER.md ./
COPY --chown=myuser:myuser SETUP_COMPLETE.txt ./

# Make scraper script executable
RUN chmod +x website_scraper.py

# Use compileall to ensure the runnability of the Actor Python code.
RUN python3 -m compileall -q src/

# Set environment variables for HTTrack
ENV HTTRACK_INSTALLED=1
ENV PATH="/usr/bin:${PATH}"

# Display versions for debugging
RUN echo "=== Environment Check ===" \
 && echo "Python: $(python --version)" \
 && echo "HTTrack: $(httrack --version | head -1)" \
 && echo "User: $(whoami)" \
 && echo "Working directory: $(pwd)" \
 && echo "======================="

# Specify how to launch the source code of your Actor.
# By default, the "python3 -m src" command is run
CMD ["python3", "-m", "src"]
