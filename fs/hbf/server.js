
// Minimal server.js for HBF integration test
const app = {};

app.handle = async function (req, res) {
    const { method, url } = req;
    if (url === "/hello" && method === "GET") {
        res.setHeader("Content-Type", "application/json");
        res.send(JSON.stringify({ message: "Hello, world!" }));
        return;
    }
    if (url.startsWith("/user/") && method === "GET") {
        const id = url.split("/")[2];
        res.setHeader("Content-Type", "application/json");
        res.send(JSON.stringify({ id }));
        return;
    }
    if (url === "/echo" && method === "GET") {
        res.setHeader("Content-Type", "application/json");
        res.send(JSON.stringify({ method, url }));
        return;
    }
    // Default: 404
    res.statusCode = 404;
    res.setHeader("Content-Type", "text/plain");
    res.send("Not Found");
};

globalThis.app = app;
