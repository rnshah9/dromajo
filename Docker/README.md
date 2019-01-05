<!--
/*-------------------------------------------------------------------------
* Copyright (C) 2018, Esperanto Technologies Inc.
* The copyright to the computer program(s) herein is the
* property of Esperanto Technologies, Inc. All Rights Reserved.
* The program(s) may be used and/or copied only with
* the written permission of Esperanto Technologies and
* in accordance with the terms and conditions stipulated in the
* agreement/contract under which the program(s) have been supplied.
*-------------------------------------------------------------------------
*/
-->


# Docker Image Organization

This folder contains a list of docker images used for different
purposes in our project.

Each folder's name specifies the Docker-image's name that is
going to be build and contains:

* docker\_wrapper\_extensions.py: Python script that has is
being loaded by the top-level esperanto.py script and
implements functionality specific to the image:
    * Adds a sub-command specific to the functionality that the image
  supports and provides additional command line arguments
    * Modifies the arguments passed to docker or the image's entrypoint
  script.
    * Modifies how the image is being built.
* Docker: Folder that contains the Dockerfile and the entrypoint
script used for building the image.


# What Is Docker

Docker is a [container technology](https://opensource.com/resources/what-docker)
that for our purposes we are using to version, distribute and deploy the
build dependencies of our code. The same Docker image should be used for
building our code both in our servers and in AWS.

# Interacting with Docker

Running a task under docker has 2 components:

* a Docker image
* a Docker container

## Docker Images
A Docker image contains the root filesystem we are going to be using.
It as the same file structure as a regular Linux distribution (it is derived
from an existing distribution's filesystem, but does not contain a kernel).
An image can be built using a Dockerfile, that describes the image's build steps
or we can download a pre-existing image from an image repository.

## Docker Containers

You can consider docker containers as an ephemeral lightweight "VM".
That means that if you need to "communicate" with the container you
need to either use a network connection, or mount a volume from the host
machine, or start an interactive session.

Note that any file changes that you make inside the container include
only files of the original image will **be lost** once you exit the container.

**If you need your changes to survive exiting the container then you need to
write them to an externally mounted volume or you need to save a new image from
the running container.**

## Leaking ROOT problem

In a number of docker images, when you instantiate a container, by default you are
running as root. This causes any files created in externally mounted volumes
to have root permissions, or user and group ID information of the user inside the
container that could not be matching any host-system user.

In our scripting infrastructure we void this problem.

# How to install docker

## AWS Instance Configured With Ansible

If you are using an AWS instance configured through an Ansible
script then docker should be already running for you

## Manual Setup

**This setup applies to manually managed AWS instances**

If you need to manually install docker in your system, then you
need to select installing the Docker-CE version

### Install Instructions
* [CentoOS](https://docs.docker.com/install/linux/docker-ce/centos/)
* [Ubuntu](https://docs.docker.com/install/linux/docker-ce/ubuntu/)


### User Permissions

Once you have installed Docker you need to be able to communicate with
the docker socket in order to interact with the Docker-daemon.

To do that add yourself to the docker group by doing

>  sudo usermod -a -G docker <YOUR_USERNAME>

After that you need to log-out and in again from your session for the
change to take effect.

Run
> groups

to validate that you have been added to the correct group

### Enable Docker Experimental Permissions

See [instructions](https://github.com/docker/docker-ce/blob/master/components/cli/experimental/README.md)
to enable the experimental docker features, they are necessary for
enabling squashing the docker images.

# Use AWS Repository

## Docker Login to AWS repo.

From the aws instructions run:

> $(aws ecr get-login --no-include-email --region us-west-1)

The above assume that you can use the AWS CLI, the configuration instructions
can be found [here](https://docs.aws.amazon.com/cli/latest/userguide/cli-chap-getting-started.html)

# Useful docker commands

 The documentation is [here](https://docs.docker.com/)

## [docker ps](https://docs.docker.com/engine/reference/commandline/ps/)

Use this command to list the currently running containers

## [docker images](https://docs.docker.com/engine/reference/commandline/images/)

Use this command to list the available images.

## [docker run](https://docs.docker.com/engine/reference/commandline/run/)

Use this command to instantiate a container. Note that the complexity of arguments
can increase significantly and that users are advised to use our wrapper
scripts instead for instantiating containers correctly.

## [docker system](https://docs.docker.com/engine/reference/commandline/system/)

Use this command to do docker system maintenance, to be used only when working
on a private instance.

## [docker kill](https://docs.docker.com/engine/reference/commandline/kill/)

Use this command to terminate a running docker instance.

# Useful docker recipes.

TO-DO
