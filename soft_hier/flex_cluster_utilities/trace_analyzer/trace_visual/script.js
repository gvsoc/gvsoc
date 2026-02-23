document.getElementById("selectFileButton").addEventListener("click", () => {
    document.getElementById("fileInput").click();
});

document.getElementById("fileInput").addEventListener("change", (event) => {
    const file = event.target.files[0];
    if (file) {
        const reader = new FileReader();
        reader.onload = (e) => {
            const lines = e.target.result.split('\n');
            const traceData = parseTrace(lines);
            displayTimeline(traceData);
        };
        reader.readAsText(file);
    }
});

function parseTrace(lines) {
    const data = {};
    const ctrl_registers = []; // Separate array for ctrl_registers events

    lines.forEach(line => {
        const match = line.match(/^(\d+):\s+(\d+):\s+\[(.*?)\]\s+(.*)$/);
        if (match) {
            const [, rawTimestamp, timestamp, path, description] = match;
            const ts = parseInt(timestamp, 10);

            if (path.includes("ctrl_registers")) {
                ctrl_registers.push({ timestamp: ts, description });
            } else {
                const clusterMatch = path.match(/\/chip\/(cluster_\d+)\/(.+?)\/trace/);
                if (clusterMatch) {
                    const cluster = clusterMatch[1];
                    const component = clusterMatch[2];

                    if (!data[cluster]) {
                        data[cluster] = { idma: [], redmule: [], vecteng: [] };
                    }

                    if (description.includes("Finished")) {
                        const [start, end] = description.match(/(\d+) ns ---> (\d+) ns/).slice(1).map(Number);

                        if (component === "idma/fe") data[cluster].idma.push({ start, end, description });
                        else if (component === "redmule") data[cluster].redmule.push({ start, end, description });
                        else if (component === "vecteng") data[cluster].vecteng.push({ start, end, description });
                    }
                }
            }
        }
    });

    return { ctrl_registers, clusters: data };
}

function displayTimeline(traceData) {
    // Clear previous SVG content
    d3.select("#timeline").selectAll("*").remove();

    const svgWidth = 800; // Fixed width for the timeline window
    const svgHeight = 600;
    const margin = { top: 20, right: 20, bottom: 40, left: 120 }; 
    const clusters = Object.keys(traceData.clusters);
    const components = ["Global Barrier"];
    clusters.forEach(cluster => {
        // components.push(`${cluster}/idma`, `${cluster}/redmule`, `${cluster}/vecteng`);
        components.push(`${cluster}/idma`, `${cluster}/redmule`);
    });

    const yScale = d3.scalePoint()
        .domain(components)
        .range([0, svgHeight])
        .padding(0.5)
        .round(true);

    const svg = d3.select("#timeline")
        .append("svg")
        .attr("width", svgWidth + margin.left + margin.right)
        .attr("height", svgHeight + margin.top + margin.bottom)
        .append("g")
        .attr("transform", `translate(${margin.left},${margin.top})`);

    const allTimestamps = traceData.ctrl_registers.map(d => d.timestamp)
        .concat(clusters.flatMap(cluster =>
            traceData.clusters[cluster].idma.map(d => d.end)
                .concat(traceData.clusters[cluster].redmule.map(d => d.end))
                // .concat(traceData.clusters[cluster].vecteng.map(d => d.end))
        ));

    const xScale = d3.scaleLinear()
        .domain([d3.min(allTimestamps), d3.max(allTimestamps)])
        .range([0, svgWidth]);

    const xAxis = svg.append("g")
        .attr("class", "x-axis")
        .attr("transform", `translate(0, ${svgHeight})`)
        .call(d3.axisBottom(xScale).ticks(10).tickFormat(d => `${d} ns`));

    svg.append("g")
        .call(d3.axisLeft(yScale));

    // Plot events with tooltips
    plotEvents(svg, traceData.ctrl_registers, xScale, yScale("ctrl_registers"));
    clusters.forEach(cluster => {
        plotProcesses(svg, traceData.clusters[cluster].idma, xScale, yScale(`${cluster}/idma`), "red");
        plotProcesses(svg, traceData.clusters[cluster].redmule, xScale, yScale(`${cluster}/redmule`), "green");
        // plotProcesses(svg, traceData.clusters[cluster].vecteng, xScale, yScale(`${cluster}/vecteng`), "purple");
    });

    // Add zoom and pan functionality
    const zoom = d3.zoom()
        .scaleExtent([1, 100]) // Set zoom level limits
        .translateExtent([[0, 0], [svgWidth, svgHeight]])
        .extent([[0, 0], [svgWidth, svgHeight]])
        .on("zoom", zoomed);

    svg.call(zoom);

    function zoomed(event) {
        const newXScale = event.transform.rescaleX(xScale);
        xAxis.call(d3.axisBottom(newXScale).ticks(10).tickFormat(d => `${Math.round(d)} ns`)); // Dynamically update ticks
        svg.selectAll(".event-circle")
            .attr("cx", d => newXScale(d.timestamp));
        svg.selectAll(".process-rect")
            .attr("x", d => newXScale(d.start))
            .attr("width", d => newXScale(d.end) - newXScale(d.start));
    }
}

// Tooltip logic
const tooltip = d3.select("#tooltip");

function plotEvents(svg, data, xScale, yPosition) {
    svg.selectAll(".event-circle")
        .data(data)
        .enter()
        .append("circle")
        .attr("class", "event-circle")
        .attr("cx", d => xScale(d.timestamp))
        .attr("cy", yPosition)
        .attr("r", 5)
        .attr("fill", "blue")
        .on("mouseover", (event, d) => {
            tooltip.style("opacity", 1).text(d.description);
        })
        .on("mousemove", (event) => {
            tooltip.style("left", (event.pageX + 10) + "px").style("top", (event.pageY - 10) + "px");
        })
        .on("mouseout", () => {
            tooltip.style("opacity", 0);
        });
}

function plotProcesses(svg, data, xScale, yPosition, color) {
    svg.selectAll(`.${color}-process`)
        .data(data)
        .enter()
        .append("rect")
        .attr("class", "process-rect")
        .attr("x", d => xScale(d.start))
        .attr("y", yPosition - 10)
        .attr("width", d => xScale(d.end) - xScale(d.start))
        .attr("height", 20)
        .attr("fill", color)
        .attr("opacity", 0.7)
        .on("mouseover", (event, d) => {
            tooltip.style("opacity", 1).text(d.description);
        })
        .on("mousemove", (event) => {
            tooltip.style("left", (event.pageX + 10) + "px").style("top", (event.pageY - 10) + "px");
        })
        .on("mouseout", () => {
            tooltip.style("opacity", 0);
        });
}
