/**
 * @file rkcdcli.go
 * @author Mikhail Klementyev jollheef<AT>riseup.net
 * @date May 2017
 * @brief user-space client proof-of-concept
 */

package main

import (
	"bufio"
	"encoding/json"
	"fmt"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
)

func readBytesUntilEOF(pipe io.ReadCloser) (buf []byte, err error) {

	bufSize := 1024

	for err != io.EOF {
		stdout := make([]byte, bufSize)
		var n int

		n, err = pipe.Read(stdout)
		if err != nil && err != io.EOF {
			return
		}

		buf = append(buf, stdout[:n]...)
	}

	if err == io.EOF {
		err = nil
	}

	return
}

func readUntilEOF(pipe io.ReadCloser) (str string, err error) {
	buf, err := readBytesUntilEOF(pipe)
	str = string(buf)
	return
}

func system(name string, arg ...string) (stdout string, stderr string,
	ret int, err error) {

	cmd := exec.Command(name, arg...)

	outPipe, err := cmd.StdoutPipe()
	if err != nil {
		return
	}

	errPipe, err := cmd.StderrPipe()
	if err != nil {
		return
	}

	cmd.Start()

	stdout, err = readUntilEOF(outPipe)
	if err != nil {
		return
	}

	stderr, err = readUntilEOF(errPipe)
	if err != nil {
		return
	}

	err = cmd.Wait()
	if err != nil {
		fmt.Sscanf(err.Error(), "exit status %d", &ret)
		return
	}

	return
}

type logEntry struct {
	ID       int
	PID      int
	TGID     int
	Comm     string
	Filename string
	Saddr    string
}

func hiddenFile(filename string) (hidden bool) {
	stdout, _, _, err := system("/bin/ls", filepath.Dir(filename))
	if err != nil {
		panic(err)
	}

	hidden = !strings.Contains(stdout, filepath.Base(filename))
	return
}

func hiddenAddr(addr string) (hidden bool) {
	stdout, _, _, err := system("/bin/netstat", "-n")
	if err != nil {
		panic(err)
	}

	hidden = !strings.Contains(stdout, addr)
	return
}

func hiddenPID(pid int) (hidden bool) {
	stdout, _, _, err := system("/bin/ps", "-eo", "pid")
	if err != nil {
		panic(err)
	}

	hidden = !strings.Contains(stdout, fmt.Sprintf("%d", pid))
	return
}

func main() {
	file, err := os.Open("/proc/rootkiticide")
	if err != nil {
		panic(err)
	}
	defer file.Close()

	reader := bufio.NewReader(file)

	files := map[string]bool{}
	addrs := map[string]bool{}
	pids := map[int]string{}

	for {
		var line []byte
		line, err = reader.ReadBytes('\n')
		if err != nil {
			break
		}
		var entry logEntry
		json.Unmarshal(line, &entry)

		if entry.Filename != "" {
			files[entry.Filename] = true
		}

		if entry.Saddr != "" {
			addrs[entry.Saddr] = true
		}

		pids[entry.PID] = entry.Comm
	}

	if err != io.EOF {
		panic(err)
	}

	fmt.Println("Hidden files (or already removed):")
	for file, _ := range files {
		if hiddenFile(file) {
			fmt.Println("\t", file)
		}
	}

	fmt.Println("Hidden connections (or already closed):")
	for addr, _ := range addrs {
		if hiddenAddr(addr) {
			fmt.Println("\t", addr)
		}
	}

	fmt.Println("Hidden processes (or already killed):")
	for pid, comm := range pids {
		if hiddenPID(pid) {
			fmt.Println("\t", pid, comm)
		}
	}

	return
}
