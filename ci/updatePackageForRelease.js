const fs = require('fs');

const NPM_PACKAGE_PATH = `${__dirname}/../src/electron/npm`;
const PACKAGE_JSON_PATH = `${NPM_PACKAGE_PATH}/package.json`;
const ELECTRON_D_TS_PATH = `${NPM_PACKAGE_PATH}/electron.d.ts`;

module.exports = async ({github}) => {
  const {VERSION} = process.env;
  const result = await github.rest.repos.getReleaseByTag({
    owner: 'coparse-inc',
    repo: 'electron-libreoffice',
    tag: VERSION,
  });

  if (!result?.data?.assets) {
    console.log('No assets');
    return;
  }

  const decoder = new TextDecoder('utf-8');

  for (const asset of result.data.assets) {
    if (asset.name === 'electron.d.ts') {
      const electronDTsData = await downloadFile(github, asset.id, decoder);
      fs.writeFileSync(ELECTRON_D_TS_PATH, electronDTsData, (err) => {
        if (err) {
          console.error(err);
        }
      });
      break;
    }
  }

  const data = await readPackageJson();

  await updatePackageJson(data, VERSION);
};

/**
 * @description Reads the package.json located in `./src/electron/npm/package.json`
 * @returns {Promise<Object>} - the JSON data of package.json
 */
async function readPackageJson() {
  return new Promise((resolve, reject) => {
    fs.readFile(PACKAGE_JSON_PATH, (err, data) => {
      // READ
      if (err) {
        reject(err);
      }
      var data = JSON.parse(data.toString());
      resolve(data);
    });
  });
}

async function downloadFile(github, assetId, decoder) {
  const res = await github.request(
    'GET /repos/{owner}/{repo}/releases/assets/{asset_id}',
    {
      headers: {
        accept: 'application/octet-stream',
      },
      owner: 'coparse-inc',
      repo: 'electron-libreoffice',
      asset_id: assetId,
    },
  );

  return decoder.decode(res.data);
}

/**
 * @description Takes the template package.json and fills it in with necessary data
 */
async function updatePackageJson(data, version) {
  const updatedData = {
    ...data,
    version,
    name: '@macro-inc/electron-libreoffice',
    repository: 'https://github.com/coparse-inc/electron-libreoffice',
    description: 'Build apps with the power of Electron AND LibreOffice',
    license: 'MIT',
    author: 'Macro, Inc.',
    keywords: ['electron', 'libreoffice'],
  };

  return new Promise((resolve, reject) => {
    fs.writeFile(PACKAGE_JSON_PATH, JSON.stringify(updatedData), (err) => {
      if (err) {
        reject(err);
      } else {
        resolve();
      }
    });
  });
}
