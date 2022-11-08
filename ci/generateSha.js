const fs = require('fs');

module.exports = async ({github}) => {
  const {VERSION} = process.env;
  // Get the release by version. This will give us the asset ids
  const result = await github.rest.repos.getReleaseByTag({
    owner: 'coparse-inc',
    repo: 'electron-libreoffice',
    tag: VERSION,
  });

  if (!result?.data?.assets) {
    console.log('No assets');
    return;
  }

  const shas = [];
  const toDelete = [];

  const decoder = new TextDecoder('utf-8');
  // For each asset id if its a sha256sum we want to download the file contents and store it
  for (const asset of result.data.assets) {
    if (asset.name.includes('.sha256sum')) {
      shas.push(await downloadFile(github, asset.id, decoder));
      toDelete.push(asset.id);
    }
  }

  // Generate txt blob
  let content = '';
  for (const sha of shas) {
    content += `${sha}\n`;
  }

  // Write file to ./ci/SHASUMS256.txt
  fs.writeFileSync(`${__dirname}/SHASUMS256.txt`, content, (err) => {
    if (err) {
      console.error(err);
    }
  });
};

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
