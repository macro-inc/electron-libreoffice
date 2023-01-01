#!/usr/bin/env python3

import argparse
import hashlib
import os
import urllib.request
import shutil
import sys
import tarfile
import time
import platform

def download(url, path):
    with urllib.request.urlopen(url) as response, open(path, 'wb') as f:
        shutil.copyfileobj(response, f)

def download_with_retry(url, out_path, tries_remaining = 5):
    while tries_remaining > 0 :
        try:
            download(url, out_path)
            print("downloaded: " + url)
            return True
        except BaseException:
            print("error downloading " + url)
            tries_remaining = tries_remaining - 1
            time.sleep(0.1)
            continue
    return False

def sha256sum(file_path):
    sha256_hash = hashlib.sha256()
    with open(file_path,"rb") as file:
        for byte_block in iter(lambda: file.read(4096),b""):
            sha256_hash.update(byte_block)
    return sha256_hash.hexdigest()

def check_valid_sha256(file_path, checksum):
    return os.path.isfile(file_path) and sha256sum(file_path) == checksum

def retrieve_checksum(url):
    return urllib.request.urlopen(
        url + '.sha256sum').read().decode('utf-8').split()[0]

def retrieve_checksum_with_retry(url, tries_remaining = 5):
    while tries_remaining > 0 :
        try:
            checksum = retrieve_checksum(url)
            print("retrieved checksum for '" + url + "': " + checksum)
            return checksum
        except BaseException:
            print("error retrieving checksum for '" + url + "'")
            tries_remaining = tries_remaining - 1
            time.sleep(0.1)
            continue

    return None

def main():
    parser = argparse.ArgumentParser(
        description="Validate, download, and extract LibreOfficeKit"
    )
    parser.add_argument(
        "--base-url", required=True, help="Base of the download URL"
    )
    parser.add_argument(
        "--os",
        required=True,
        choices=['win', 'linux', 'mac'],
        help="Target operating system"
    )
    parser.add_argument(
        "--version", required=True,
        help="version release tag (ex: window-nightly-run-33)"
    )
    parser.add_argument(
        "--output-dir", required=True, help="output directory"
    )
    args = parser.parse_args()

    # 'output_dir' must exist and be a directory.
    if args.output_dir is not None:
        args.output_dir = os.path.abspath(args.output_dir)

    # safety check, to ensure nothing other than libreofficekit is removed
    if not args.output_dir.endswith('libreofficekit'):
        parser.error("--output-dir must end with libreofficekit: {}".format(
                         args.output_dir))

    # macs support more than one architecture, since DEPS does not support
    # target_arch assume it is the host's arch
    if args.os == 'mac':
        args.os = 'mac-' + platform.machine()
    filename = 'libreofficekit-' + args.os + '.tar.xz'
    url = args.base_url + '/' + args.version + '/' + filename
    checksum = retrieve_checksum_with_retry(url)
    if checksum is None:
        parser.error("unable to retriveve checksum for '{}'".format(url))

    output_file = os.path.join(args.output_dir, filename)
    # /program directory already exists and the archive is the same checksum,
    # exit early
    if check_valid_sha256(output_file, checksum):
        if os.path.isdir(os.path.join(args.output_dir, 'instdir')):
            print("file '{}' already exists with checksum {}".format(
                      filename, checksum))
            return 0
        else:
            print("extracting '{}'".format(output_file))
            with tarfile.open(output_file) as f:
                f.extractall(args.output_dir)
            return 0

    # remove the existing output_dir
    if os.path.isdir(args.output_dir):
        shutil.rmtree(args.output_dir)

    os.makedirs(args.output_dir)

    print("downloading '{}'".format(url))

    if not download_with_retry(url, output_file):
        parser.error("unable to download '{}'".format(url))

    if not check_valid_sha256(output_file, checksum):
        parser.error("file '{}' does not match checksum {}".format(
                         filename, checksum))

    print("extracting '{}'".format(output_file))
    with tarfile.open(output_file) as f:
        f.extractall(args.output_dir)

    return 0


if __name__ == "__main__":
    sys.exit(main())
