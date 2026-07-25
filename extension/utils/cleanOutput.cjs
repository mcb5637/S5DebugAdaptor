const fs = require("fs");

const folderName = "out";
try {
  fs.rm(folderName, { recursive: true, force: false }, (err) => {
    if (err != undefined) {
      if (err.code !== "ENOENT") {
        throw new Error(err.message);
      }
    }
  });
} catch (err) {
  return 1
}
return 0;