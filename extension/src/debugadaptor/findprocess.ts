import { exec } from "child_process";
import * as net from "net";

export async function checkProcessAndPort(processName: string, port: number) {
  if (!(await checkProcess(processName))) {
    return false;
  }
  //return true
  if (!(await checkPort(port))) {
    return false;
  }
  return true;
}

async function checkPort(port: number) {
   const isWindows = process.platform === "win32";
   const command = isWindows
    ? `netstat -ano | findstr /R /C:":19021" | find /c /v ""`
    : `ss -ano | grep ":19021" | wc -l`;
      return new Promise((resolve) => {
      exec(command, (error, stdout) => {
        resolve(+stdout > 0)
      })
  });
  
}

async function checkProcess(processName: string) {
  const isWindows = process.platform === "win32";
  const command = isWindows
    ? `tasklist /FI "IMAGENAME eq ${processName}"`
    : `pgrep -x "${processName}"`;

  return new Promise((resolve) => {
    exec(command, (error, stdout) => {
      if (isWindows) {
        const isRunning = stdout.toLowerCase().includes(processName.toLowerCase());
        resolve(isRunning);
      } else {
        const isRunning = !error;
        resolve(isRunning);
      }
    });
  });
}
