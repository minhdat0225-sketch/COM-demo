/**
 * html2pdf.js — Puppeteer script to convert HTML to PDF
 * Usage: node html2pdf.js <input.html> [output.pdf]
 * 
 * Features:
 *   - Waits for Mermaid diagrams to fully render
 *   - Respects CSS page-break rules
 *   - A4 format with proper margins
 *   - Header/Footer with page numbers
 */
const puppeteer = require('puppeteer');
const path = require('path');

(async () => {
    const inputFile = process.argv[2];
    if (!inputFile) {
        console.error('Usage: node html2pdf.js <input.html> [output.pdf]');
        process.exit(1);
    }

    const inputPath = path.resolve(inputFile);
    const outputPath = process.argv[3]
        ? path.resolve(process.argv[3])
        : inputPath.replace(/\.html?$/i, '.pdf');

    console.log(`📄 Input:  ${inputPath}`);
    console.log(`📦 Output: ${outputPath}`);

    const browser = await puppeteer.launch({
        headless: 'new',
        args: ['--no-sandbox', '--disable-setuid-sandbox']
    });

    const page = await browser.newPage();

    // Navigate to the HTML file
    await page.goto(`file://${inputPath}`, {
        waitUntil: 'networkidle0',
        timeout: 30000
    });

    // Wait for Mermaid diagrams to render (they replace .mermaid divs with SVGs)
    console.log('⏳ Waiting for Mermaid diagrams to render...');
    try {
        await page.waitForFunction(() => {
            const mermaidDivs = document.querySelectorAll('.mermaid');
            if (mermaidDivs.length === 0) return true;
            // Mermaid replaces content with SVG; check all have SVG inside
            return Array.from(mermaidDivs).every(
                div => div.querySelector('svg') !== null || div.getAttribute('data-processed') === 'true'
            );
        }, { timeout: 15000 });
        console.log('✅ All Mermaid diagrams rendered.');
    } catch (e) {
        console.warn('⚠️  Mermaid render timeout — proceeding anyway.');
    }

    // Extra wait for any CSS transitions / reflows
    await new Promise(r => setTimeout(r, 2000));

    // Generate PDF
    console.log('🖨️  Generating PDF...');
    await page.pdf({
        path: outputPath,
        format: 'A4',
        printBackground: true,
        preferCSSPageSize: true,
        margin: {
            top: '2cm',
            bottom: '2cm',
            left: '2cm',
            right: '2cm'
        },
        displayHeaderFooter: true,
        headerTemplate: `
            <div style="width:100%; font-size:8px; padding:0 1cm; display:flex; justify-content:space-between; color:#999;">
                <span>HALA Academy</span>
                <span>System Architect Automotive</span>
            </div>`,
        footerTemplate: `
            <div style="width:100%; font-size:8px; text-align:center; color:#999;">
                <span class="pageNumber"></span> / <span class="totalPages"></span>
            </div>`
    });

    await browser.close();
    console.log(`✅ PDF saved: ${outputPath}`);
})();
