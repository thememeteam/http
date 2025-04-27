const server = Bun.serve({
    port: 3000,
    async fetch(request) {
      const t = await request.text();
      return new Response(t);
    },
  });
  
  console.log(`Listening on ${server.url}`);