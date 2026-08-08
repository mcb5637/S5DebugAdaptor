const fs = require('fs');
const path = require('path');

const srcDir = path.join(__dirname, '..','src', 'public');
const destDir = path.join(__dirname, '..','out');

function copyDir(src, dest) {
    fs.mkdirSync(dest, { recursive: true });
    const entries = fs.readdirSync(src, { withFileTypes: true });

    for (const entry of entries) {
        const srcPath = path.join(src, entry.name);
        const destPath = path.join(dest, entry.name);
        if (entry.isDirectory()) {
            copyDir(srcPath, destPath);
        } else {
            fs.copyFileSync(srcPath, destPath);
        }
    }
}

try {
    if (fs.existsSync(srcDir)) {
        copyDir(srcDir, destDir);
    } else {
        throw new Error('source directory "src/public" not existing.')
    }
} catch (err) {
    console.error('Error on copying:', err);
}
